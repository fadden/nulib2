/*
 * Nulib2
 * Copyright (C) 2000 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 *
 * Filename manipulation, including file type preservation.
 */
#include "Nulib2.h"
#include <ctype.h>


/*
 * ===========================================================================
 *      Common definitions
 * ===========================================================================
 */

#define kPreserveIndic  '#'     /* use # rather than $ for hex indication */
#define kFilenameExtDelim   '.'     /* separates extension from filename */
#define kResourceFlag   'r'
#define kDiskImageFlag  'i'
#define kMaxExtLen      4       /* ".1234" */
#define kResourceStr    "_rsrc_"

/* must be longer then strlen(kResourceStr)... no problem there */
#define kMaxPathGrowth  (sizeof("#XXXXXXXXYYYYYYYYZ")-1 + kMaxExtLen+1)


/* ProDOS file type names; must be entirely in upper case */
static const char gFileTypeNames[256][4] = {
    "NON", "BAD", "PCD", "PTX", "TXT", "PDA", "BIN", "FNT",
    "FOT", "BA3", "DA3", "WPF", "SOS", "$0D", "$0E", "DIR",
    "RPD", "RPI", "AFD", "AFM", "AFR", "SCL", "PFS", "$17",
    "$18", "ADB", "AWP", "ASP", "$1C", "$1D", "$1E", "$1F",
    "TDM", "$21", "$22", "$23", "$24", "$25", "$26", "$27",
    "$28", "$29", "8SC", "8OB", "8IC", "8LD", "P8C", "$2F",
    "$30", "$31", "$32", "$33", "$34", "$35", "$36", "$37",
    "$38", "$39", "$3A", "$3B", "$3C", "$3D", "$3E", "$3F",
    "DIC", "OCR", "FTD", "$43", "$44", "$45", "$46", "$47",
    "$48", "$49", "$4A", "$4B", "$4C", "$4D", "$4E", "$4F",
    "GWP", "GSS", "GDB", "DRW", "GDP", "HMD", "EDU", "STN",
    "HLP", "COM", "CFG", "ANM", "MUM", "ENT", "DVU", "FIN",
    "$60", "$61", "$62", "$63", "$64", "$65", "$66", "$67",
    "$68", "$69", "$6A", "BIO", "$6C", "TDR", "PRE", "HDV",
    "$70", "$71", "$72", "$73", "$74", "$75", "$76", "$77",
    "$78", "$79", "$7A", "$7B", "$7C", "$7D", "$7E", "$7F",
    "$80", "$81", "$82", "$83", "$84", "$85", "$86", "$87",
    "$88", "$89", "$8A", "$8B", "$8C", "$8D", "$8E", "$8F",
    "$90", "$91", "$92", "$93", "$94", "$95", "$96", "$97",
    "$98", "$99", "$9A", "$9B", "$9C", "$9D", "$9E", "$9F",
    "WP ", "$A1", "$A2", "$A3", "$A4", "$A5", "$A6", "$A7",
    "$A8", "$A9", "$AA", "GSB", "TDF", "BDF", "$AE", "$AF",
    "SRC", "OBJ", "LIB", "S16", "RTL", "EXE", "PIF", "TIF",
    "NDA", "CDA", "TOL", "DVR", "LDF", "FST", "$BE", "DOC",
    "PNT", "PIC", "ANI", "PAL", "$C4", "OOG", "SCR", "CDV",
    "FON", "FND", "ICN", "$CB", "$CC", "$CD", "$CE", "$CF",
    "$D0", "$D1", "$D2", "$D3", "$D4", "MUS", "INS", "MDI",
    "SND", "$D9", "$DA", "DBM", "$DC", "DDD", "$DE", "$DF",
    "LBR", "$E1", "ATK", "$E3", "$E4", "$E5", "$E6", "$E7",
    "$E8", "$E9", "$EA", "$EB", "$EC", "$ED", "R16", "PAS",
    "CMD", "$F1", "$F2", "$F3", "$F4", "$F5", "$F6", "$F7",
    "$F8", "OS ", "INT", "IVR", "BAS", "VAR", "REL", "SYS"
};

/*
 * Some file extensions we recognize.  When adding files with "extended"
 * preservation mode, we try to assign types to files that weren't
 * explicitly preserved, but nevertheless have a recognizeable type.
 *
 * geoff@gwlink.net points out that this really ought to be in an external
 * file rather than a hard-coded table.  Ought to fix that someday.
 */
static const struct {
    const char*         label;
    ushort              fileType;
    ulong               auxType;
    uchar               flags;
} gRecognizedExtensions[] = {
    { "ASM",  0xb0, 0x0003, 0 },        /* APW assembly source */
    { "C",    0xb0, 0x000a, 0 },        /* APW C source */
    { "H",    0xb0, 0x000a, 0 },        /* APW C header */
    { "BNY",  0xe0, 0x8000, 0 },        /* Binary II lib */
    { "BQY",  0xe0, 0x8000, 0 },        /* Binary II lib, w/ compress */
    { "BXY",  0xe0, 0x8000, 0 },        /* Binary II wrap around SHK */
    { "BSE",  0xe0, 0x8000, 0 },        /* Binary II wrap around SEA */
    { "SEA",  0xb3, 0xdb07, 0 },        /* GSHK SEA */
    { "GIF",  0xc0, 0x8006, 0 },        /* GIF image */
    { "JPG",  0x06, 0x0000, 0 },        /* JPEG (nicer than 'NON') */
    { "JPEG", 0x06, 0x0000, 0 },        /* JPEG (nicer than 'NON') */
    { "SHK",  0xe0, 0x8002, 0 },        /* ShrinkIt archive */
};


/*
 * Return a pointer to the three-letter representation of the file type name.
 */
const char*
GetFileTypeString(ulong fileType)
{
    if (fileType < NELEM(gFileTypeNames))
        return gFileTypeNames[fileType];
    else
        return "???";
}


/*
 * ===========================================================================
 *      File type preservation
 * ===========================================================================
 */

/*
 * Add a preservation string.
 *
 * "pathBuf" is assumed to have enough space to hold the current path
 * plus kMaxPathGrowth more.  It will be modified in place.
 */
static void
AddPreservationString(NulibState* pState,
    const NuPathnameProposal* pPathProposal, char* pathBuf)
{
    char extBuf[kMaxPathGrowth +1];
    const NuRecord* pRecord;
    const NuThread* pThread;
    NuThreadID threadID;
    char* cp;

    assert(pState != nil);
    assert(pPathProposal != nil);
    assert(pathBuf != nil);
    assert(NState_GetModPreserveType(pState));

    pRecord = pPathProposal->pRecord;
    pThread = pPathProposal->pThread;
    assert(pRecord != nil);
    assert(pThread != nil);

    cp = extBuf;

    /*
     * Cons up a preservation string.  On some platforms "sprintf" doesn't
     * return the #of characters written, so we add it up manually.
     */
    if (pRecord->recFileType < 0x100 && pRecord->recExtraType < 0x10000) {
        sprintf(cp, "%c%02lx%04lx", kPreserveIndic, pRecord->recFileType,
            pRecord->recExtraType);
        cp += 7;
    } else {
        sprintf(cp, "%c%08lx%08lx", kPreserveIndic, pRecord->recFileType,
            pRecord->recExtraType);
        cp += 17;
    }

    threadID = NuMakeThreadID(pThread->thThreadClass, pThread->thThreadKind);
    if (threadID == kNuThreadIDRsrcFork)
        *cp++ = kResourceFlag;
    else if (threadID == kNuThreadIDDiskImage)
        *cp++ = kDiskImageFlag;

    /*
     * If they've asked for "extended" type preservation, then we need
     * to retain either the existing extension or append an extension
     * based on the ProDOS file type.
     */
    if (NState_GetModPreserveTypeExtended(pState)) {
        const char* pExt;
        char* end;

        /*
         * Find extension.  Note FindExtension guarantees there's at least
         * one char after '.'.
         *
         * It's hard to know when this is right and when it isn't.  It's
         * fairly likely that a text file really ought to end in ".txt",
         * and it's fairly unlikely that a BIN file should be ".bin", but
         * where do the rest fall in?  We might want to force TXT files
         * to be ".txt", and perhaps do something clever for some others.
         */
        if (pRecord->recFileType == 0x04)
            pExt = nil;
        else
            pExt = FindExtension(pState, pathBuf);
        if (pExt != nil && strlen(pExt+1) <= kMaxExtLen) {
            pExt++; /* skip past the '.' */

            /* if it's strictly decimal-numeric, don't use it (.1, .2, etc) */
            (void) strtoul(pExt, &end, 10);
            if (*end == '\0') {
                pExt = nil;
            } else {
                /* if '#' appears in it, don't use it -- it'll confuse us */
                const char* ccp = pExt;
                while (*ccp != '\0') {
                    if (*ccp == '#') {
                        pExt = nil;
                        break;
                    }
                    ccp++;
                }
            }

        } else {
            /*
             * There's no extension on the filename.  Use the standard
             * ProDOS type, if one exists for this entry.  We don't use
             * the table if it's "NON" or a hex value.
             */
            if (pRecord->recFileType) {
                pExt = GetFileTypeString(pRecord->recFileType);
                if (pExt[0] == '?' || pExt[0] == '$')
                    pExt = nil;
            }
        }

        if (pExt != nil) {
            *cp++ = kFilenameExtDelim;
            strcpy(cp, pExt);
            cp += strlen(pExt);
        }
    }

    /* make sure it's terminated */
    *cp = '\0';

    assert(cp - extBuf <= kMaxPathGrowth);
    strcat(pathBuf, extBuf);
}

/*
 * Normalize a path for the conventions on the output filesystem.  This
 * adds optional file type preservation.
 *
 * The path from the archive is in "pPathProposal".  Thew new pathname
 * will be placed in the "new pathname" section of "pPathProposal".
 *
 * The new pathname may be shorter (because characters were removed) or
 * longer (if we add a "#XXYYYYZ" extension or replace chars with '%' codes).
 *
 * This returns the new pathname, which is held in NState's temporary
 * pathname buffer.
 */
const char*
NormalizePath(NulibState* pState, NuPathnameProposal* pPathProposal)
{
    NuError err = kNuErrNone;
    char* pathBuf;
    const char* startp;
    const char* endp;
    char* dstp;
    char localFssep;

    assert(pState != nil);
    assert(pPathProposal != nil);
    assert(pPathProposal->pathname != nil);

    localFssep = NState_GetSystemPathSeparator(pState);

    /*
     * Set up temporary buffer space.  The maximum possible expansion
     * requires converting all chars to '%' codes and adding the longest
     * possible preservation string.
     */
    NState_SetTempPathnameLen(pState,
        strlen(pPathProposal->pathname)*3 + kMaxPathGrowth +1);
    pathBuf = NState_GetTempPathnameBuf(pState);
    assert(pathBuf != nil);
    if (pathBuf == nil)
        return nil;

    startp = pPathProposal->pathname;
    dstp = pathBuf;
    while (*startp == pPathProposal->filenameSeparator) {
        /* ignore leading path sep; always extract to current dir */
        startp++;
    }

    /* normalize all directory components and the filename component */
    while (startp != nil) {
        endp = strchr(startp, pPathProposal->filenameSeparator);
        if (endp != nil) {
            /* normalize directory component */
            err = NormalizeDirectoryName(pState, startp, endp - startp,
                    pPathProposal->filenameSeparator, &dstp,
                    NState_GetTempPathnameLen(pState));
            if (err != kNuErrNone)
                goto bail;

            *dstp++ = localFssep;

            startp = endp +1;
        } else {
            /* normalize filename */
            err = NormalizeFileName(pState, startp, strlen(startp),
                    pPathProposal->filenameSeparator, &dstp,
                    NState_GetTempPathnameLen(pState));
            if (err != kNuErrNone)
                goto bail;

            /* add/replace extension if necessary */
            *dstp++ = '\0';
            if (NState_GetModPreserveType(pState)) {
                AddPreservationString(pState, pPathProposal, pathBuf);
            } else if (NuGetThreadID(pPathProposal->pThread) == kNuThreadIDRsrcFork)
            {
                /* add this in lieu of the preservation extension */
                strcat(pathBuf, kResourceStr);
            }

            startp = nil;   /* we're done */
        }
    }

    pPathProposal->newPathname = pathBuf;
    pPathProposal->newFilenameSeparator = localFssep;

    /* check for overflow */
    assert(dstp - pathBuf <=
                    (int)(strlen(pPathProposal->pathname) + kMaxPathGrowth));

    /*
     * If "junk paths" is set, drop everything but the last component.
     */
    if (NState_GetModJunkPaths(pState)) {
        char* lastFssep;
        lastFssep = strrchr(pathBuf, localFssep);
        if (lastFssep != nil) {
            assert(*(lastFssep+1) != '\0'); /* should already have been caught*/
            memmove(pathBuf, lastFssep+1, strlen(lastFssep+1)+1);
        }
    }

bail:
    if (err != kNuErrNone)
        return nil;
    return pathBuf;
}


/*
 * ===========================================================================
 *      File type restoration
 * ===========================================================================
 */

/*
 * Try to figure out what file type is associated with a filename extension.
 *
 * This checks the standard list of ProDOS types (which should catch things
 * like "TXT" and "BIN") and the separate list of recognized extensions.
 */
static void
LookupExtension(NulibState* pState, const char* ext, ulong* pFileType,
    ulong* pAuxType)
{
    char uext3[4];
    int i, extLen;

    extLen = strlen(ext);
    assert(extLen > 0);

    /*
     * First step is to try to find it in the recognized types list.
     */
    for (i = 0; i < NELEM(gRecognizedExtensions); i++) {
        if (strcasecmp(ext, gRecognizedExtensions[i].label) == 0) {
            *pFileType = gRecognizedExtensions[i].fileType;
            *pAuxType = gRecognizedExtensions[i].auxType;
            goto bail;
        }
    }

    /*
     * Second step is to try to find it in the ProDOS types list.
     *
     * The extension is converted to upper case and padded with spaces.
     *
     * [do we want to obstruct matching on things like '$f7' here?]
     */
    if (extLen <= 3) {
        for (i = 2; i >= extLen; i--)
            uext3[i] = ' ';
        for ( ; i >= 0; i--)
            uext3[i] = toupper(ext[i]);
        uext3[3] = '\0';

        /*printf("### converted '%s' to '%s'\n", ext, uext3);*/

        for (i = 0; i < NELEM(gFileTypeNames); i++) {
            if (strcmp(uext3, gFileTypeNames[i]) == 0) {
                *pFileType = i;
                goto bail;
            }
        }
    }

bail:
    return;
}

/*
 * Try to associate some meaning with the file extension.
 */
void
InterpretExtension(NulibState* pState, const char* pathName, ulong* pFileType,
    ulong* pAuxType)
{
    const char* pExt;

    assert(pState != nil);
    assert(pathName != nil);
    assert(pFileType != nil);
    assert(pAuxType != nil);

    pExt = FindExtension(pState, pathName);
    if (pExt != nil)
        LookupExtension(pState, pExt+1, pFileType, pAuxType);
}


/*
 * Check to see if there's a preservation string on the filename.  If so,
 * set the filetype and auxtype information, and trim the preservation
 * string off.
 *
 * We have to be careful not to trip on false-positive occurrences of '#'
 * in the filename.
 */
Boolean
ExtractPreservationString(NulibState* pState, char* pathname, ulong* pFileType,
    ulong* pAuxType, NuThreadID* pThreadID)
{
    char numBuf[9];
    ulong fileType, auxType;
    NuThreadID threadID;
    char* pPreserve;
    char* cp;
    int digitCount;

    assert(pState != nil);
    assert(pathname != nil);
    assert(pFileType != nil);
    assert(pAuxType != nil);
    assert(pThreadID != nil);

    pPreserve = strrchr(pathname, kPreserveIndic);
    if (pPreserve == nil)
        return false;

    /* count up the #of hex digits */
    digitCount = 0;
    for (cp = pPreserve+1; *cp != '\0' && isxdigit((int)*cp); cp++)
        digitCount++;

    /* extract the file and aux type */
    switch (digitCount) {
    case 6:
        /* ProDOS 1-byte type and 2-byte aux */
        memcpy(numBuf, pPreserve+1, 2);
        numBuf[2] = 0;
        fileType = strtoul(numBuf, &cp, 16);
        assert(cp == numBuf + 2);

        auxType = strtoul(pPreserve+3, &cp, 16);
        assert(cp == pPreserve + 7);
        break;
    case 16:
        /* HFS 4-byte type and 4-byte creator */
        memcpy(numBuf, pPreserve+1, 8);
        numBuf[8] = 0;
        fileType = strtoul(numBuf, &cp, 16);
        assert(cp == numBuf + 8);

        auxType = strtoul(pPreserve+9, &cp, 16);
        assert(cp == pPreserve + 17);
        break;
    default:
        /* not valid */
        return false;
    }

    /* check for a threadID specifier */
    threadID = kNuThreadIDDataFork;
    switch (*cp) {
    case kResourceFlag:
        threadID = kNuThreadIDRsrcFork;
        cp++;
        break;
    case kDiskImageFlag:
        threadID = kNuThreadIDDiskImage;
        cp++;
        break;
    default:
        /* do nothing... yet */
        break;
    }

    /* make sure we were the very last component */
    switch (*cp) {
    case kFilenameExtDelim:     /* redundant "-ee" extension */
    case '\0':                  /* end of string! */
        break;
    default:
        return false;
    }

    /* truncate the original string, and return what we got */
    *pPreserve = '\0';
    *pFileType = fileType;
    *pAuxType = auxType;
    *pThreadID = threadID;

    return true;
}


/*
 * Remove NuLib2's normalization magic (e.g. "%2f" for '/').
 *
 * This always results in the filename staying the same length or getting
 * smaller, so we can do it in place in the buffer.
 */
void
DenormalizePath(NulibState* pState, char* pathBuf)
{
    const char* srcp;
    char* dstp;
    char ch;

    srcp = pathBuf;
    dstp = pathBuf;

    while (*srcp != '\0') {
        if (*srcp == kForeignIndic) {
            srcp++;
            if (*srcp == kForeignIndic) {
                *dstp++ = kForeignIndic;
                srcp++;
            } else if (isxdigit((int)*srcp)) {
                ch = HexDigit(*srcp) << 4;
                srcp++;
                if (isxdigit((int)*srcp)) {
                    /* valid, output char */
                    ch += HexDigit(*srcp);
                    *dstp++ = ch;
                    srcp++;
                } else {
                    /* bogus '%' with trailing hex digit found! */
                    *dstp++ = kForeignIndic;
                    *dstp++ = *(srcp-1);
                }
            } else {
                /* bogus lone '%s' found! */
                *dstp++ = kForeignIndic;
            }

        } else {
            *dstp++ = *srcp++;
        }
    }

    *dstp = '\0';
    assert(dstp <= srcp);
}


/*
 * ===========================================================================
 *      Misc utils
 * ===========================================================================
 */

/*
 * Find the filename component of a local pathname.  Uses the fssep defined
 * in pState.
 *
 * Always returns a pointer to a string; never returns nil.
 */
const char*
FilenameOnly(NulibState* pState, const char* pathname)
{
    const char* retstr;
    const char* pSlash;
    char* tmpStr = nil;

    assert(pState != nil);
    assert(pathname != nil);

    pSlash = strrchr(pathname, NState_GetSystemPathSeparator(pState));
    if (pSlash == nil) {
        retstr = pathname;      /* whole thing is the filename */
        goto bail;
    }

    pSlash++;
    if (*pSlash == '\0') {
        if (strlen(pathname) < 2) {
            retstr = pathname;  /* the pathname is just "/"?  Whatever */
            goto bail;
        }

        /* some bonehead put an fssep on the very end; back up before it */
        /* (not efficient, but this should be rare, and I'm feeling lazy) */
        tmpStr = strdup(pathname);
        tmpStr[strlen(pathname)-1] = '\0';
        pSlash = strrchr(tmpStr, NState_GetSystemPathSeparator(pState));

        if (pSlash == nil) {
            retstr = pathname;  /* just a filename with a '/' after it */
            goto bail;
        }

        pSlash++;
        if (*pSlash == '\0') {
            retstr = pathname;  /* I give up! */
            goto bail;
        }

        retstr = pathname + (pSlash - tmpStr);

    } else {
        retstr = pSlash;
    }

bail:
    Free(tmpStr);
    return retstr;
}

/*
 * Return the filename extension found in a full pathname.
 *
 * An extension is the stuff following the last '.' in the filename.  If
 * there is nothing following the last '.', then there is no extension.
 *
 * Returns a pointer to the '.' preceding the extension, or nil if no
 * extension was found.
 */
const char*
FindExtension(NulibState* pState, const char* pathname)
{
    const char* pFilename;
    const char* pExt;

    /*
     * We have to isolate the filename so that we don't get excited
     * about "/foo.bar/file".
     */
    pFilename = FilenameOnly(pState, pathname);
    assert(pFilename != nil);
    pExt = strrchr(pFilename, kFilenameExtDelim);

    /* also check for "/blah/foo.", which doesn't count */
    if (pExt != nil && *(pExt+1) != '\0')
        return pExt;

    return nil;
}

