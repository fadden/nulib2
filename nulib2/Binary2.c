/*
 * Nulib2
 * Copyright (C) 2000 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 *
 * Binary II support.
 *
 * The BNY format is significantly different from the NuFX format, so
 * support wasn't included in NufxLib.  Since there's no reason to
 * create BNY archives when SHK is available, I've only implemented
 * data extraction features.
 *
 * (Technically, BNY isn't an archive format.  It was a way to encapsulate
 * file attributes and file data for transmission over a modem, and was
 * meant to be added and stripped on the fly.)
 */
#include "Nulib2.h"

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifndef O_BINARY
# define O_BINARY   0
#endif

/* how to open a file */
#ifdef FOPEN_WANTS_B
# define kFileOpenReadOnly      "rb"
#else
# define kFileOpenReadOnly      "r"
#endif


/*
 * ===========================================================================
 *      Utility functions
 * ===========================================================================
 */

/*
 * Open a file read-only.  We abstract this so we get "r" vs "rb" right.
 * (Moves this to SysUtils.c if anybody else needs it.)
 */
NuError
OpenFileReadOnly(const char* filename, FILE** pFp)
{
    NuError err = kNuErrNone;

    *pFp = fopen(filename, kFileOpenReadOnly);
    if (*pFp == nil)
        err = errno ? errno : kNuErrFileOpen;

    return err;
}

/*
 * Determine whether the current file we're examining is described by
 * the file specification given on the command line.
 *
 * If no filespec was provided, then all records are "specified".
 */
Boolean
NameIsSpecified(NulibState* pState, const char* filename)
{
    char* const* pSpec;
    int i;

    if (!NState_GetFilespecCount(pState))
        return true;

    pSpec = NState_GetFilespecPointer(pState);
    for (i = NState_GetFilespecCount(pState); i > 0; i--, pSpec++) {
        if (NState_GetModRecurse(pState)) {
            if (strncmp(*pSpec, filename, strlen(*pSpec)) == 0)
                return true;
        } else {
            if (strcmp(*pSpec, filename) == 0)
                return true;
        }
    }

    return false;
}


/*
 * ===========================================================================
 *      Binary II goodies
 * ===========================================================================
 */

/*
 * State for the currently open Binary II archive.
 */
typedef struct BNYArchive {
    NulibState*     pState;
    FILE*           fp;
    Boolean         first;
} BNYArchive;


#define kBNYBlockSize       128         /* BNY files are broken into blocks */
#define kBNYMaxFileName     64
#define kBNYMaxNativeName   48
#define kBNYFlagCompressed  (1<<7)
#define kBNYFlagEncrypted   (1<<6)
#define kBNYFlagSparse      (1)

/*
 * An entry in a Binary II archive.  Each archive is essentially a stream
 * of files; only the "filesToFollow" value gives any indication that
 * something else follows this entry.
 */
typedef struct BNYEntry {
    ushort          access;
    ushort          fileType;
    ulong           auxType;
    uchar           storageType;
    ulong           fileSize;           /* in 512-byte blocks */
    ushort          prodosModDate;
    ushort          prodosModTime;
    NuDateTime      modWhen;            /* computed from previous two fields */
    ushort          prodosCreateDate;
    ushort          prodosCreateTime;
    NuDateTime      createWhen;         /* computed from previous two fields */
    ulong           eof;
    ulong           realEOF;            /* eof is bogus for directories */
    char            fileName[kBNYMaxFileName+1];
    char            nativeName[kBNYMaxNativeName+1];
    ulong           diskSpace;          /* in 512-byte blocks */
    uchar           osType;             /* not exactly same as NuFileSysID */
    ushort          nativeFileType;
    uchar           phantomFlag;
    uchar           dataFlags;          /* advisory flags */
    uchar           version;
    uchar           filesToFollow;      /* #of files after this one */

    uchar           blockBuf[kBNYBlockSize];
} BNYEntry;

/*
 * Test for the magic number on a file in SQueezed format.
 */
static inline Boolean
IsSqueezed(uchar one, uchar two)
{
    return (one == 0x76 && two == 0xff);
}

/*
 * Test if this entry is a directory.
 */
static inline Boolean
IsDir(BNYEntry* pEntry)
{
    /*
     * NuLib and "unblu.c" compared against file type 15 (DIR), so I'm
     * going to do that too, but it would probably be better to compare
     * against storageType 0x0d.
     */
    return (pEntry->fileType == 15);
}


/*
 * Initialize a BNYArchive structure.
 */
static BNYArchive*
BNYInit(NulibState* pState)
{
    BNYArchive* pBny;

    pBny = Malloc(sizeof(*pBny));
    memset(pBny, 0, sizeof(*pBny));

    pBny->pState = pState;

    return pBny;
}

/*
 * Free up a BNYArchive, disposing of anything inside it.
 */
static void
BNYFree(BNYArchive* pBny)
{
    /* don't need to do this on stdin, but won't really hurt */
    if (pBny->fp != nil)
        fclose(pBny->fp);

    Free(pBny);
}


/*
 * Open a Binary II archive read-only.  Might be a file on disk or a
 * stream on stdin.
 */
static NuError
BNYOpenReadOnly(BNYArchive* pBny)
{
    NuError err = kNuErrNone;
    NulibState* pState;
    
    Assert(pBny != nil);
    Assert(pBny->pState != nil);
    Assert(pBny->fp == nil);

    pState = pBny->pState;

    if (IsFilenameStdin(NState_GetArchiveFilename(pState))) {
        pBny->fp = stdin;
        /*
         * Since the archive is on stdin, we can't ask the user questions.
         * On a UNIX system we could open /dev/tty, but that's not portable,
         * and I don't think archives on stdin are going to be popular
         * enough to make this worth doing.
         */
        NState_SetInputUnavailable(pState, true);
    } else {
        err = OpenFileReadOnly(NState_GetArchiveFilename(pState), &pBny->fp);
        if (err != kNuErrNone) {
            ReportError(err, "unable to open '%s'",
                NState_GetArchiveFilename(pState));
            goto bail;
        }
    }

bail:
    return err;
}


/*
 * Wrapper for fread().  Note the arguments resemble read(2) rather
 * than fread(3S).
 */
static NuError
BNYRead(BNYArchive* pBny, void* buf, size_t nbyte)
{
    size_t result;

    Assert(pBny != nil);
    Assert(buf != nil);
    Assert(nbyte > 0);
    Assert(pBny->fp != nil);

    errno = 0;
    result = fread(buf, 1, nbyte, pBny->fp);
    if (result != nbyte)
        return errno ? errno : kNuErrFileRead;
    return kNuErrNone;
}

/*
 * Seek within an archive.  Because we need to handle streaming archives,
 * and don't need to special-case anything, we only allow relative
 * forward seeks.
 */
static NuError
BNYSeek(BNYArchive* pBny, long offset)
{
    Assert(pBny != nil);
    Assert(pBny->fp != nil);
    Assert(pBny->pState != nil);
    Assert(offset > 0);

    /*DBUG(("--- seeking forward %ld bytes\n", offset));*/

    if (IsFilenameStdin(NState_GetArchiveFilename(pBny->pState))) {
        /* OPT: might be faster to fread a chunk at a time */
        while (offset--)
            (void) getc(pBny->fp);

        if (ferror(pBny->fp) || feof(pBny->fp))
            return kNuErrFileSeek;
    } else {
        if (fseek(pBny->fp, offset, SEEK_CUR) < 0)
            return kNuErrFileSeek;
    }
    
    return kNuErrNone;
}


/*
 * Convert from ProDOS compact date format to the expanded DateTime format.
 */
static void
BNYConvertDateTime(ushort prodosDate, ushort prodosTime, NuDateTime* pWhen)
{
    pWhen->second = 0;
    pWhen->minute = prodosTime & 0x3f;
    pWhen->hour = (prodosTime >> 8) & 0x1f;
    pWhen->day = prodosDate & 0x1f;
    pWhen->month = (prodosDate >> 5) & 0x0f;
    pWhen->year = (prodosDate >> 9) & 0x7f;
    if (pWhen->year < 40)
        pWhen->year += 100;     /* P8 uses 0-39 for 2000-2039 */
    pWhen->extra = 0;
    pWhen->weekDay = 0;
}

/*
 * Decode a Binary II header.
 *
 * See the File Type Note for $e0/8000 to decipher the buffer offsets
 * and meanings.
 */
static NuError
BNYDecodeHeader(BNYArchive* pBny, BNYEntry* pEntry)
{
    NuError err = kNuErrNone;
    uchar* raw;
    int len;

    Assert(pBny != nil);
    Assert(pEntry != nil);

    raw = pEntry->blockBuf;

    if (raw[0] != 0x0a || raw[1] != 0x47 || raw[2] != 0x4c || raw[18] != 0x02) {
        err = kNuErrBadData;
        ReportError(err, "this doesn't look like a Binary II header");
        goto bail;
    }

    pEntry->access = raw[3] | raw[111] << 8;
    pEntry->fileType = raw[4] | raw[112] << 8;
    pEntry->auxType = raw[5] | raw[6] << 8 | raw[109] << 16 | raw[110] << 24;
    pEntry->storageType = raw[7];
    pEntry->fileSize = raw[8] | raw[9] << 8;
    pEntry->prodosModDate = raw[10] | raw[11] << 8;
    pEntry->prodosModTime = raw[12] | raw[13] << 8;
    BNYConvertDateTime(pEntry->prodosModDate, pEntry->prodosModTime,
        &pEntry->modWhen);
    pEntry->prodosCreateDate = raw[14] | raw[15] << 8;
    pEntry->prodosCreateTime = raw[16] | raw[17] << 8;
    BNYConvertDateTime(pEntry->prodosCreateDate, pEntry->prodosCreateTime,
        &pEntry->createWhen);
    pEntry->eof = raw[20] | raw[21] << 8 | raw[22] << 16 | raw[116] << 24;
    len = raw[23];
    if (len > kBNYMaxFileName) {
        err = kNuErrBadData;
        ReportError(err, "invalid filename length %d", len);
        goto bail;
    }
    memcpy(pEntry->fileName, &raw[24], len);
    pEntry->fileName[len] = '\0';

    pEntry->nativeName[0] = '\0';
    if (len <= 15 && raw[39] != 0) {
        len = raw[39];
        if (len > kBNYMaxNativeName) {
            err = kNuErrBadData;
            ReportError(err, "invalid filename length %d", len);
            goto bail;
        }
        memcpy(pEntry->nativeName, &raw[40], len);
        pEntry->nativeName[len] = '\0';
    }

    pEntry->diskSpace = raw[117] | raw[118] << 8 | raw[119] << 16 |
                        raw[120] << 24;

    pEntry->osType = raw[121];
    pEntry->nativeFileType = raw[122] | raw[123] << 8;
    pEntry->phantomFlag = raw[124];
    pEntry->dataFlags = raw[125];
    pEntry->version = raw[126];
    pEntry->filesToFollow = raw[127];

    /* directories are given an EOF but don't actually have any content */
    if (IsDir(pEntry))
        pEntry->realEOF = 0;
    else
        pEntry->realEOF = pEntry->eof;

bail:
    return err;
}


/*
 * Normalize the pathname by running it through the usual NuLib2
 * function.  The trick here is that the function usually takes a
 * NuPathnameProposal, which we don't happen to have handy.  Rather
 * than generalize the NuLib2 code, we just create a fake proposal,
 * which is a bit dicey but shouldn't break too easily.
 *
 * This takes care of -e, -ee, and -j.
 *
 * We return the new path, which is stored in NulibState's temporary
 * filename buffer.
 */
const char*
BNYNormalizePath(BNYArchive* pBny, BNYEntry* pEntry)
{
    NuPathnameProposal pathProposal;
    NuRecord fakeRecord;
    NuThread fakeThread;

    /* make uninitialized data obvious */
    memset(&fakeRecord, 0xa1, sizeof(fakeRecord));
    memset(&fakeThread, 0xa5, sizeof(fakeThread));

    pathProposal.pathname = pEntry->fileName;
    pathProposal.filenameSeparator = '/';   /* BNY always uses ProDOS conv */
    pathProposal.pRecord = &fakeRecord;
    pathProposal.pThread = &fakeThread;

    pathProposal.newPathname = nil;
    pathProposal.newFilenameSeparator = '\0';
    pathProposal.newDataSink = nil;

    /* need the filetype and auxtype for -e/-ee */
    fakeRecord.recFileType = pEntry->fileType;
    fakeRecord.recExtraType = pEntry->auxType;

    /* need the components of a ThreadID */
    fakeThread.thThreadClass = kNuThreadClassData;
    fakeThread.thThreadKind = 0x0000;       /* data fork */

    return NormalizePath(pBny->pState, &pathProposal);
}


/*
 * Copy all data from the Binary II file to "outfp", reading in 128-byte
 * blocks.
 *
 * Uses pEntry->blockBuf, which already has the first 128 bytes in it.
 */
static NuError
BNYCopyBlocks(BNYArchive* pBny, BNYEntry* pEntry, FILE* outfp)
{
    NuError err = kNuErrNone;
    long bytesLeft;

    Assert(pEntry->realEOF > 0);

    bytesLeft = pEntry->realEOF;
    while (bytesLeft > 0) {
        long toWrite;

        toWrite = bytesLeft;
        if (toWrite > kBNYBlockSize)
            toWrite = kBNYBlockSize;

        if (outfp != nil) {
            if (fwrite(pEntry->blockBuf, toWrite, 1, outfp) != 1) {
                err = errno ? errno : kNuErrFileWrite;
                ReportError(err, "BNY write failed");
                goto bail;
            }
        }

        bytesLeft -= toWrite;

        if (bytesLeft) {
            err = BNYRead(pBny, pEntry->blockBuf, kBNYBlockSize);
            if (err != kNuErrNone) {
                ReportError(err, "BNY read failed");
                goto bail;
            }
        }
    }

bail:
    return err;
}


/*
 * ===========================================================================
 *      Unsqueeze
 * ===========================================================================
 */

/*
 * This was ripped fairly directly from Squeeze.c in NufxLib.  Because
 * there's relatively little code, and providing direct access to the
 * compression functions is a little unwieldy, I've cut & pasted the
 * necessary pieces here.
 */
#define FULL_SQ_HEADER
#define kSqBufferSize 8192          /* must be enough to hold full SQ header */

#define kNuSQMagic      0xff76      /* magic value for file header */
#define kNuSQRLEDelim   0x90        /* RLE delimiter */
#define kNuSQEOFToken   256         /* distinguished stop symbol */
#define kNuSQNumVals    257         /* 256 symbols + stop */


/*
 * State during uncompression.
 */
typedef struct USQState {
    ulong           dataInBuffer;
    uchar*          dataPtr;
    int             bitPosn;
    int             bits;

    /*
     * Decoding tree; first "nodeCount" values are populated.  Positive
     * values are indicies to another node in the tree, negative values
     * are literals (+1 because "negative zero" doesn't work well).
     */
    int             nodeCount;
    struct {
        short       child[2];       /* left/right kids, must be signed 16-bit */
    } decTree[kNuSQNumVals-1];
} USQState;


/*
 * Decode the next symbol from the Huffman stream.
 */
static NuError
USQDecodeHuffSymbol(USQState* pUsqState, int* pVal)
{
    short val = 0;
    int bits, bitPosn;

    bits = pUsqState->bits;     /* local copy */
    bitPosn = pUsqState->bitPosn;

    do {
        if (++bitPosn > 7) {
            /* grab the next byte and use that */
            bits = *pUsqState->dataPtr++;
            bitPosn = 0;
            if (!pUsqState->dataInBuffer--)
                return kNuErrBufferUnderrun;

            val = pUsqState->decTree[val].child[1 & bits];
        } else {
            /* still got bits; shift right and use it */
            val = pUsqState->decTree[val].child[1 & (bits >>= 1)];
        }
    } while (val >= 0);

    /* val is negative literal; add one to make it zero-based then negate it */
    *pVal = -(val + 1);

    pUsqState->bits = bits;
    pUsqState->bitPosn = bitPosn;

    return kNuErrNone;
}


/*
 * Read two bytes of signed data out of the buffer.
 */
static inline NuError
USQReadShort(USQState* pUsqState, short* pShort)
{
    if (pUsqState->dataInBuffer < 2)
        return kNuErrBufferUnderrun;

    *pShort = *pUsqState->dataPtr++;
    *pShort |= (*pUsqState->dataPtr++) << 8;
    pUsqState->dataInBuffer -= 2;

    return kNuErrNone;
}

/*
 * Expand "SQ" format.
 *
 * Because we have a stop symbol, knowing the uncompressed length of
 * the file is not essential.
 */
static NuError
BNYUnSqueeze(BNYArchive* pBny, BNYEntry* pEntry, FILE* outfp)
{
    NuError err = kNuErrNone;
    USQState usqState;
    ulong compRemaining, getSize;
#ifdef FULL_SQ_HEADER
    ushort magic, fileChecksum, checksum;
#endif
    short nodeCount;
    int i, inrep;
    uchar* tmpBuf = nil;
    uchar lastc = 0;

    tmpBuf = Malloc(kSqBufferSize);
    if (tmpBuf == nil) {
        err = kNuErrMalloc;
        goto bail;
    }

    usqState.dataInBuffer = 0;
    usqState.dataPtr = tmpBuf;

    compRemaining = pEntry->realEOF;
#ifdef FULL_SQ_HEADER
    if (compRemaining < 8)
#else
    if (compRemaining < 3)
#endif
    {
        err = kNuErrBadData;
        ReportError(err, "too short to be valid SQ data");
        goto bail;
    }

    /*
     * Round up to the nearest 128-byte boundary.  We need to read
     * everything out of the file in case this is a streaming archive.
     * Because the compressed data has an embedded stop symbol, it's okay
     * to "overrun" the expansion code.
     */
    compRemaining =
        ((compRemaining + kBNYBlockSize-1) / kBNYBlockSize) * kBNYBlockSize;

    /* want to grab up to kSqBufferSize bytes */
    if (compRemaining > kSqBufferSize)
        getSize = kSqBufferSize;
    else
        getSize = compRemaining;

    /* copy the <= 128 bytes we already have into the general buffer */
    memcpy(usqState.dataPtr, pEntry->blockBuf, kBNYBlockSize);
    if (getSize > kBNYBlockSize) {
        getSize -= kBNYBlockSize;
        compRemaining -= kBNYBlockSize;
        usqState.dataInBuffer += kBNYBlockSize;
    } else {
        Assert(compRemaining <= kBNYBlockSize);
        usqState.dataInBuffer = getSize;
        getSize = 0;
        compRemaining = 0;
    }

    /* temporarily advance dataPtr past the block we copied in */
    usqState.dataPtr += kBNYBlockSize;

    /*
     * Grab a big chunk.  "compRemaining" is the amount of compressed
     * data left in the file, usqState.dataInBuffer is the amount of
     * compressed data left in the buffer.
     *
     * We always want to read 128-byte blocks.
     */
    if (getSize) {
        Assert(getSize <= kSqBufferSize);
        err = BNYRead(pBny, usqState.dataPtr, getSize);
        if (err != kNuErrNone) {
            ReportError(err,
                "failed reading compressed data (%ld bytes)", getSize);
            goto bail;
        }
        usqState.dataInBuffer += getSize;
        if (getSize > compRemaining)
            compRemaining = 0;
        else
            compRemaining -= getSize;
    }

    /* reset dataPtr */
    usqState.dataPtr = tmpBuf;

    /*
     * Read the header.  We assume that the header will fit in the
     * compression buffer ( sq allowed 300+ for the filename, plus
     * 257*2 for the tree, plus misc).
     */
    Assert(kSqBufferSize > 1200);
#ifdef FULL_SQ_HEADER
    err = USQReadShort(&usqState, &magic);
    if (err != kNuErrNone)
        goto bail;
    if (magic != kNuSQMagic) {
        err = kNuErrBadData;
        ReportError(err, "bad magic number in SQ block");
        goto bail;
    }

    err = USQReadShort(&usqState, &fileChecksum);
    if (err != kNuErrNone)
        goto bail;

    checksum = 0;

    /* skip over the filename */
    while (*usqState.dataPtr++ != '\0')
        usqState.dataInBuffer--;
    usqState.dataInBuffer--;
#endif

    err = USQReadShort(&usqState, &nodeCount);
    if (err != kNuErrNone)
        goto bail;
    if (nodeCount < 0 || nodeCount >= kNuSQNumVals) {
        err = kNuErrBadData;
        ReportError(err, "invalid decode tree in SQ (%d nodes)", nodeCount);
        goto bail;
    }
    usqState.nodeCount = nodeCount;

    /* initialize for possibly empty tree (only happens on an empty file) */
    usqState.decTree[0].child[0] = -(kNuSQEOFToken+1);
    usqState.decTree[0].child[1] = -(kNuSQEOFToken+1);

    /* read the nodes, ignoring "read errors" until we're done */
    for (i = 0; i < nodeCount; i++) {
        err = USQReadShort(&usqState, &usqState.decTree[i].child[0]);
        err = USQReadShort(&usqState, &usqState.decTree[i].child[1]);
    }
    if (err != kNuErrNone) {
        err = kNuErrBadData;
        ReportError(err, "SQ data looks truncated at tree");
        goto bail;
    }

    usqState.bitPosn = 99;      /* force an immediate read */

    /*
     * Start pulling data out of the file.  We have to Huffman-decode
     * the input, and then feed that into an RLE expander.
     *
     * A completely lopsided (and broken) Huffman tree could require
     * 256 tree descents, so we want to try to ensure we have at least 256
     * bits in the buffer.  Otherwise, we could get a false buffer underrun
     * indication back from DecodeHuffSymbol.
     *
     * The SQ sources actually guarantee that a code will fit entirely
     * in 16 bits, but there's no reason not to use the larger value.
     */
    inrep = false;
    while (1) {
        int val;

        if (usqState.dataInBuffer < 65 && compRemaining) {
            /*
             * Less than 256 bits, but there's more in the file.
             *
             * First thing we do is slide the old data to the start of
             * the buffer.
             */
            if (usqState.dataInBuffer) {
                Assert(tmpBuf != usqState.dataPtr);
                memmove(tmpBuf, usqState.dataPtr, usqState.dataInBuffer);
            }
            usqState.dataPtr = tmpBuf;

            /*
             * Next we read as much as we can.
             */
            if (kSqBufferSize - usqState.dataInBuffer < compRemaining)
                getSize = kSqBufferSize - usqState.dataInBuffer;
            else
                getSize = compRemaining;

            Assert(getSize <= kSqBufferSize);
            err = BNYRead(pBny, usqState.dataPtr + usqState.dataInBuffer,
                    getSize);
            if (err != kNuErrNone) {
                ReportError(err,
                    "failed reading compressed data (%ld bytes)", getSize);
                goto bail;
            }
            usqState.dataInBuffer += getSize;
            if (getSize > compRemaining)
                compRemaining = 0;
            else
                compRemaining -= getSize;

            Assert(compRemaining < 32767*65536);
            Assert(usqState.dataInBuffer <= kSqBufferSize);
        }

        err = USQDecodeHuffSymbol(&usqState, &val);
        if (err != kNuErrNone) {
            ReportError(err, "failed decoding huff symbol");
            goto bail;
        }

        if (val == kNuSQEOFToken)
            break;

        /*
         * Feed the symbol into the RLE decoder.
         */
        if (inrep) {
            /*
             * Last char was RLE delim, handle this specially.  We use
             * --val instead of val-- because we already emitted the
             * first occurrence of the char (right before the RLE delim).
             */
            if (val == 0) {
                /* special case -- just an escaped RLE delim */
                lastc = kNuSQRLEDelim;
                val = 2;
            }
            while (--val) {
                /*if (pCrc != nil)
                    *pCrc = Nu_CalcCRC16(*pCrc, &lastc, 1);*/
                if (outfp != nil)
                    putc(lastc, outfp);
                #ifdef FULL_SQ_HEADER
                checksum += lastc;
                #endif
            }
            inrep = false;
        } else {
            /* last char was ordinary */
            if (val == kNuSQRLEDelim) {
                /* set a flag and catch the count the next time around */
                inrep = true;
            } else {
                lastc = val;
                /*if (pCrc != nil)
                    *pCrc = Nu_CalcCRC16(*pCrc, &lastc, 1);*/
                if (outfp != nil)
                    putc(lastc, outfp);
                #ifdef FULL_SQ_HEADER
                checksum += lastc;
                #endif
            }
        }

    }

    if (inrep) {
        err = kNuErrBadData;
        ReportError(err, "got stop symbol when run length expected");
        goto bail;
    }

    #ifdef FULL_SQ_HEADER
    /* verify the checksum stored in the SQ file */
    if (checksum != fileChecksum) {
        err = kNuErrBadDataCRC;
        ReportError(err, "expected 0x%04x, got 0x%04x (SQ)",
            fileChecksum, checksum);
        goto bail;
    } else {
        DBUG(("--- SQ checksums match (0x%04x)\n", checksum));
    }
    #endif

    /*
     * Gobble up any unused bytes in the last 128-byte block.  There
     * shouldn't be more than that left over.
     */
    if (compRemaining > kSqBufferSize) {
        err = kNuErrBadData;
        ReportError(err, "wow: found %ld bytes left over", compRemaining);
        goto bail;
    }
    if (compRemaining) {
        DBUG(("+++ slurping up last %ld bytes\n", compRemaining));
        err = BNYRead(pBny, tmpBuf, compRemaining);
        if (err != kNuErrNone) {
            ReportError(err, "failed reading leftovers");
            goto bail;
        }
    }

bail:
    if (outfp != nil)
        fflush(outfp);
    Free(tmpBuf);
    return err;
}


/*
 * ===========================================================================
 *      Iterators
 * ===========================================================================
 */

typedef NuError (*BNYIteratorFunc)(BNYArchive* pBny, BNYEntry* pEntry,
                                    Boolean* consumedFlag);

/*
 * Iterate through a Binary II archive, calling "func" to perform
 * operations on the file.
 */
static NuError
BNYIterate(NulibState* pState, BNYIteratorFunc func)
{
    NuError err = kNuErrNone;
    BNYArchive* pBny = nil;
    BNYEntry entry;
    Boolean consumed;
    int first = true;
    int toFollow;

    Assert(pState != nil);
    Assert(func != nil);

    NState_SetMatchCount(pState, 0);

    pBny = BNYInit(pState);
    if (pBny == nil) {
        err = kNuErrMalloc;
        goto bail;
    }

    err = BNYOpenReadOnly(pBny);
    if (err != kNuErrNone)
        goto bail;

    toFollow = 1;       /* assume 1 file in archive */
    pBny->first = true;
    while (toFollow) {
        err = BNYRead(pBny, entry.blockBuf, sizeof(entry.blockBuf));
        if (err != kNuErrNone) {
            ReportError(err, "failed while reading header in '%s'",
                NState_GetArchiveFilename(pState));
            goto bail;
        }

        err = BNYDecodeHeader(pBny, &entry);
        if (err != kNuErrNone) {
            if (first)
                ReportError(err, "not a Binary II archive?");
            goto bail;
        }

        /*
         * If the file has one or more blocks, read the first block now.
         * This will allow the various functions to evaluate the file
         * contents for SQueeze compression.
         */
        if (entry.realEOF != 0) {
            err = BNYRead(pBny, entry.blockBuf, sizeof(entry.blockBuf));
            if (err != kNuErrNone) {
                ReportError(err, "failed while reading '%s'",
                    NState_GetArchiveFilename(pState));
                goto bail;
            }
        }

        /*
         * Invoke the appropriate function if this file was requested.
         */
        consumed = false;
        if (NameIsSpecified(pBny->pState, entry.fileName)) {
            NState_IncMatchCount(pState);

            err = (*func)(pBny, &entry, &consumed);
            if (err != kNuErrNone)
                goto bail;

            pBny->first = false;
        }

        /*
         * If they didn't "consume" the entire BNY entry, we need to
         * do it for them.  We've already read the first block (if it
         * existed), so we don't need to eat that one again.
         */
        if (!consumed) {
            int nblocks = (entry.realEOF + kBNYBlockSize-1) / kBNYBlockSize;

            if (nblocks > 1) {
                err = BNYSeek(pBny, (nblocks-1) * kBNYBlockSize);
                if (err != kNuErrNone) {
                    ReportError(err, "failed while seeking forward");
                    goto bail;
                }
            }
        }

        if (!first) {
            if (entry.filesToFollow != toFollow -1) {
                ReportError(kNuErrNone,
                    "WARNING: filesToFollow %d, expected %d\n",
                    entry.filesToFollow, toFollow -1);
            }
        }
        toFollow = entry.filesToFollow;

        first = false;
    }

    if (!NState_GetMatchCount(pState))
        printf("%s: no records match\n", gProgName);

bail:
    if (pBny != nil)
        BNYFree(pBny);
    if (err != kNuErrNone) {
        DBUG(("--- Iterator returning failure %d\n", err));
    }
    return err;
}

/*
 * Get a quick table of contents.
 */
static NuError
BNYListShort(BNYArchive* pBny, BNYEntry* pEntry, Boolean* pConsumedFlag)
{
    NuError err = kNuErrNone;

    printf("%s\n", pEntry->fileName);

    return err;
}

/*
 * Get a verbose listing of contents.
 */
static NuError
BNYListVerbose(BNYArchive* pBny, BNYEntry* pEntry, Boolean* pConsumedFlag)
{
    NuError err = kNuErrNone;
    Boolean isSqueezed, isReadOnly;
    NulibState* pState;
    char date1[kDateOutputLen];
    int len;

    pState = pBny->pState;

    if (pBny->first) {
        const char* ccp;

        if (IsFilenameStdin(NState_GetArchiveFilename(pState)))
            ccp = "<stdin>";
        else
            ccp = FilenameOnly(pState, NState_GetArchiveFilename(pState));

        printf("%-59.59s Files:%5u\n\n",
            ccp, pEntry->filesToFollow+1);

        printf(" Name                        Type Auxtyp Modified"
               "         Fmat   Length\n");
        printf("-------------------------------------------------"
               "----------------------\n");
    }

    isSqueezed = false;
    if (pEntry->realEOF && IsSqueezed(pEntry->blockBuf[0], pEntry->blockBuf[1]))
        isSqueezed = true;

    len = strlen(pEntry->fileName);
    isReadOnly = pEntry->access == 0x21L || pEntry->access == 0x01L;
    if (len <= 27) {
        printf("%c%-27.27s ",
            isReadOnly ? '+' : ' ', pEntry->fileName);
    } else {
        printf("%c..%-25.25s ",
            isReadOnly ? '+' : ' ', pEntry->fileName + len - 25);
    }
    printf("%s  $%04lX  ",
        GetFileTypeString(pEntry->fileType), pEntry->auxType);

    printf("%s  ", FormatDateShort(&pEntry->modWhen, date1));
    if (isSqueezed)
        printf("squ  ");
    else
        printf("unc  ");

    printf("%8ld", pEntry->realEOF);

    printf("\n");

    if (!pEntry->filesToFollow) {
        /* last entry, print footer */
        printf("-------------------------------------------------"
               "----------------------\n");
    }

    return err;
}

/*
 * Get a verbose table of contents.
 */
static NuError
BNYListDebug(BNYArchive* pBny, BNYEntry* pEntry, Boolean* pConsumedFlag)
{
    NuError err = kNuErrNone;

    printf("File name: '%s'  Native name: '%s'  BNY Version %d\n",
        pEntry->fileName, pEntry->nativeName,
        pEntry->version);
    printf("  Phantom: %s  DataFlags: 0x%02x (%s%s%s)\n",
        pEntry->phantomFlag ? "YES" : "no",
        pEntry->dataFlags,
        pEntry->dataFlags & kBNYFlagCompressed ? "compr" : "",
        pEntry->dataFlags & kBNYFlagEncrypted ? "encry" : "",
        pEntry->dataFlags & kBNYFlagSparse ? "sparse" : "");
    printf("  Modified %d/%02d/%02d %02d:%02d  Created %d/%02d/%02d %02d:%02d\n",
        pEntry->modWhen.year+1900, pEntry->modWhen.month,
        pEntry->modWhen.day, pEntry->modWhen.hour,
        pEntry->modWhen.minute,
        pEntry->createWhen.year+1900, pEntry->createWhen.month,
        pEntry->createWhen.day, pEntry->createWhen.hour,
        pEntry->createWhen.minute);
    printf("  FileType: 0x%04x  AuxType: 0x%08lx  StorageType: 0x%02x\n",
        pEntry->fileType, pEntry->auxType, pEntry->storageType);
    printf("  EOF: %ld  FileSize: %ld blocks  DiskSpace: %ld blocks\n",
        pEntry->eof, pEntry->fileSize, pEntry->diskSpace);
    printf("  Access: 0x%04x  OSType: %d  NativeFileType: 0x%04x\n",
        pEntry->access, pEntry->osType, pEntry->nativeFileType);
    if (pEntry->realEOF) {
        printf("  *File begins 0x%02x 0x%02x%s\n",
            pEntry->blockBuf[0], pEntry->blockBuf[1],
            IsSqueezed(pEntry->blockBuf[0], pEntry->blockBuf[1]) ?
                " (probably SQueezed)" : "");
    }
    printf("  FilesToFollow: %d\n", pEntry->filesToFollow);

    return err;
}

/* quick enum to simplify our lives a bit */
typedef enum { kBNYExtNormal, kBNYExtPipe, kBNYExtTest } ExtMode;

/*
 * Handle "extraction" of a directory.
 */
static NuError
BNYExtractDirectory(BNYArchive* pBny, BNYEntry* pEntry, ExtMode extMode)
{
    NuError err = kNuErrNone;
    const char* newName;
    Boolean isDir;
    const char* actionStr = "HOSED";

    if (extMode == kBNYExtTest) {
        actionStr = "skipping ";
    } else if (NState_GetModJunkPaths(pBny->pState)) {
        actionStr = "skipping  ";
    } else {
        /*
         * Using the normalized name of a directory is a problem
         * when "-e" is set.  Since Binary II officially only
         * allows ProDOS names, and everything under the sun that
         * supports "long" filenames can handle 15 chars of
         * [A-Z][a-z][0-9][.], I'm going to skip the normalization
         * step.
         *
         * The "right" way to handle this is to ignore the directory
         * entries and just create any directories for the fully
         * normalized pathname generated below.
         */
        /*newName = BNYNormalizePath(pBny, pEntry);*/
        newName = pEntry->fileName;
        if (newName == nil)
            goto bail;

        err = TestFileExistence(newName, &isDir);
        if (err == kNuErrNone) {
            if (isDir) {
                actionStr = "skipping  ";
            } else {
                err = kNuErrFileExists;
                ReportError(err,
                    "unable to create directory '%s'", newName);
                goto bail;
            }
        } else if (err == kNuErrFileNotFound) {
            err = Mkdir(newName);

            if (err == kNuErrNone) {
                actionStr = "creating  ";
            } else {
                ReportError(err, "failed creating directory '%s'", newName);
            }
        }
    }

    if (!NState_GetSuppressOutput(pBny->pState)) {
        printf("\rDONE %s  %s  (directory)\n", actionStr, pEntry->fileName);
    }

bail:
    return err;
}


/*
 * Handle "extract", "extract to pipe", and "test".
 */
static NuError
BNYExtract(BNYArchive* pBny, BNYEntry* pEntry, Boolean* pConsumedFlag)
{
    NuError err = kNuErrNone;
    NulibState* pState;
    ExtMode extMode;
    const char* actionStr = "HOSED";
    FILE* outfp = nil;
    Boolean eolConv;

    pState = pBny->pState;

    switch (NState_GetCommand(pState)) {
    case kCommandExtract:           extMode = kBNYExtNormal;    break;
    case kCommandExtractToPipe:     extMode = kBNYExtPipe;      break;
    case kCommandTest:              extMode = kBNYExtTest;      break;
    default:
        err = kNuErrInternal;
        ReportError(err, "unexpected command %d in BNYExtract",
            NState_GetCommand(pState));
        Assert(0);
        goto bail;
    }

    /*eolConv = NState_GetModConvertAll(pState);*/
    eolConv = false;        /* maybe someday */

    /*
     * Binary II requires that all directories be listed explicitly.
     * If we see one, create it.
     */
    if (IsDir(pEntry)) {
        err = BNYExtractDirectory(pBny, pEntry, extMode);
        goto bail;
    }

    /*
     * Open the file, taking various command line flags into account.
     * If we're writing to a pipe, just use that.  If we're in test
     * mode, the output goes nowhere.
     */
    if (extMode == kBNYExtPipe) {
        outfp = stdout;
    } else if (extMode == kBNYExtNormal) {
        /*
         * Normalize the filename.  If the file is squeezed, and it ends
         * with ".QQ", strip off the .QQ part.  (The SQ format actually
         * includes a filename within, but it could be confusing to use
         * that instead of the name in the BNY archive, so I've decided to
         * ignore it.)
         */
        const char* newName;
        Boolean isDir;
        char* ext;

        /* if we find .qq on a squeezed file, just stomp the copy in pEntry */
        if (strlen(pEntry->fileName) > 3) {
            ext = pEntry->fileName + strlen(pEntry->fileName) -3;
            if (IsSqueezed(pEntry->blockBuf[0], pEntry->blockBuf[1]) &&
                strcasecmp(ext, ".qq") == 0)
            {
                *ext = '\0';
            }
        }

        newName = BNYNormalizePath(pBny, pEntry);
        if (newName == nil)
            goto bail;

        err = TestFileExistence(newName, &isDir);
        if (err == kNuErrNone) {
            /* file exists, what to do? */
            if (isDir) {
                err = kNuErrFileExists;
                ReportError(err, "unable to replace directory '%s'", newName);
                goto bail;
            }

            if (!NState_GetModOverwriteExisting(pState)) {
                err = kNuErrFileExists;
                ReportError(err, "unable to overwrite '%s' (try '-s'?)",
                    newName);
                goto bail;
            }
        } else if (err != kNuErrFileNotFound) {
            ReportError(err, "stat failed on '%s'", newName);
            goto bail;
        } else {
            Assert(err == kNuErrFileNotFound);
            err = kNuErrNone;
        }

        /* open it, overwriting anything present */
        outfp = fopen(newName, "w");
        if (outfp == nil) {
            err = kNuErrFileOpen;
            goto bail;
        }
    } else {
        /* outfp == nil means we're in test mode */
        Assert(outfp == nil);
    }

    /*
     * Show initial progress update message ("extracting" or "expanding",
     * depending on whether or not the file was squeezed).
     */
    if (IsSqueezed(pEntry->blockBuf[0], pEntry->blockBuf[1]))
        actionStr = "expanding ";
    else
        actionStr = "extracting";
    if (extMode == kBNYExtTest)
        actionStr = "verifying";
    if (!NState_GetSuppressOutput(pState)) {
        printf("\r  0%% %s%c %s", actionStr, eolConv ? '+' : ' ',
            pEntry->fileName);
    }

    /*
     * Extract the file.  Send the output, perhaps with EOL conversion,
     * to the output file.
     *
     * Thought for the day: assuming a file is Squeezed based only on
     * the magic number is bogus.  If we get certain classes of failures,
     * and this isn't a streaming archive, we should back up and just
     * extract it as a plain file.
     *
     * If the file is empty, don't do anything.
     */
    if (pEntry->realEOF) {
        if (IsSqueezed(pEntry->blockBuf[0], pEntry->blockBuf[1]))
            err = BNYUnSqueeze(pBny, pEntry, outfp);
        else
            err = BNYCopyBlocks(pBny, pEntry, outfp);

        if (err != kNuErrNone)
            goto bail;
    }

    /*
     * Show final progress update.
     */
    if (!NState_GetSuppressOutput(pState)) {
        printf("\rDONE\n");
    }

    *pConsumedFlag = true;

bail:
    if (outfp != nil && outfp != stdout)
        fclose(outfp);
    return err;
}


/*
 * ===========================================================================
 *      Entry points from NuLib2
 * ===========================================================================
 */

NuError
BNYDoExtract(NulibState* pState)
{
    if (NState_GetModConvertText(pState) ||
        NState_GetModConvertAll(pState))
    {
        fprintf(stderr,
            "%s: Binary II extraction doesn't support '-l' or '-ll'\n",
            gProgName); 
        return kNuErrSyntax;
    }
    if (NState_GetModUpdate(pState) ||
        NState_GetModFreshen(pState))
    {
        fprintf(stderr,
            "%s: Binary II extraction doesn't support '-f' or '-u'\n",
            gProgName);
        return kNuErrSyntax;
    }
    if (NState_GetModComments(pState))
    {
        fprintf(stderr,
            "%s: Binary II extraction doesn't support '-c'\n",
            gProgName);
        return kNuErrSyntax;
    }

    if (NState_GetCommand(pState) == kCommandExtractToPipe)
        NState_SetSuppressOutput(pState, true);
    return BNYIterate(pState, BNYExtract);
}

NuError
BNYDoTest(NulibState* pState)
{
    return BNYIterate(pState, BNYExtract);
}

NuError
BNYDoListShort(NulibState* pState)
{
    return BNYIterate(pState, BNYListShort);
}

NuError
BNYDoListVerbose(NulibState* pState)
{
    return BNYIterate(pState, BNYListVerbose);
}

NuError
BNYDoListDebug(NulibState* pState)
{
    return BNYIterate(pState, BNYListDebug);
}

