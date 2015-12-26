/*
 * NuLib2
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.
 *
 * System-dependent utility functions.
 */
#include "NuLib2.h"

#ifdef HAVE_WINDOWS_H
# include <windows.h>
#endif
#ifdef MAC_LIKE
# include <sys/xattr.h>
#endif

/* get a grip on this opendir/readdir stuff */
#if defined(UNIX_LIKE)
#  if defined(HAVE_DIRENT_H)
#    include <dirent.h>
#    define DIR_NAME_LEN(dirent)    ((int)strlen((dirent)->d_name))
     typedef struct dirent DIR_TYPE;
#  elif defined(HAVE_SYS_DIR_H)
#    include <sys/dir.h>
#    define DIR_NAME_LEN(direct)    ((direct)->d_namlen)
     typedef struct direct DIR_TYPE;
#  elif defined(HAVE_NDIR_H)
#    include <sys/ndir.h>
#    define DIR_NAME_LEN(direct)    ((direct)->d_namlen)
     typedef struct direct DIR_TYPE;
#  else
#    error "Port this?"
#  endif
#endif

/*
 * For systems (e.g. Visual C++ 6.0) that don't have these standard values.
 */
#ifndef S_IRUSR
# define S_IRUSR    0400
# define S_IWUSR    0200
# define S_IXUSR    0100
# define S_IRWXU    (S_IRUSR|S_IWUSR|S_IXUSR)
# define S_IRGRP    (S_IRUSR >> 3)
# define S_IWGRP    (S_IWUSR >> 3)
# define S_IXGRP    (S_IXUSR >> 3)
# define S_IRWXG    (S_IRWXU >> 3)
# define S_IROTH    (S_IRGRP >> 3)
# define S_IWOTH    (S_IWGRP >> 3)
# define S_IXOTH    (S_IXGRP >> 3)
# define S_IRWXO    (S_IRWXG >> 3)
#endif
#ifndef S_ISREG
# define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
# define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif


/*
 * ===========================================================================
 *      System-specific filename stuff
 * ===========================================================================
 */

// 20150103: this makes me nervous
#define kTempFileNameLen    20


#if defined(UNIX_LIKE)

/*
 * Filename normalization for typical UNIX filesystems.  Only '/' is
 * forbidden.  Maximum filename length is large enough that we might
 * as well just let the filesystem truncate if it gets too long, rather
 * than worry about truncating it cleverly.
 */
static NuError UNIXNormalizeFileName(NulibState* pState, const char* srcp,
    long srcLen, char fssep, char** pDstp, long dstLen)
{
    char* dstp = *pDstp;

    while (srcLen--) {      /* don't go until null found! */
        Assert(*srcp != '\0');

        if (*srcp == kForeignIndic) {
            /* change '%' to "%%" */
            if (NState_GetModPreserveType(pState))
                *dstp++ = *srcp;
            *dstp++ = *srcp++;
        } else if (*srcp == '/') {
            /* change '/' to "%2f" */
            if (NState_GetModPreserveType(pState)) {
                *dstp++ = kForeignIndic;
                *dstp++ = HexConv(*srcp >> 4 & 0x0f);
                *dstp++ = HexConv(*srcp & 0x0f);
            } else {
                *dstp++ = '_';
            }
            srcp++;
        } else {
            /* no need to fiddle with it */
            *dstp++ = *srcp++;
        }
    }

    *dstp = '\0';       /* end the string, but don't advance past the null */
    Assert(*pDstp - dstp <= dstLen);    /* make sure we didn't overflow */
    *pDstp = dstp;

    return kNuErrNone;
}

#elif defined(WINDOWS_LIKE)
/*
 * You can't create files or directories with these names on a FAT filesystem,
 * because they're MS-DOS "device special files".  So, we either prepend
 * a '_' (if we're not preserving filenames) or "%00" (if we are).  The
 * "%00" sequence gets stripped off during denormalization.
 *
 * For some reason FAT is actually even more picky than that, insisting
 * that files like "CON.FOO.TXT" are also illegal.
 *
 * The list comes from the Linux kernel's fs/msdos/namei.c.
 */
static const char* fatReservedNames3[] = {
    "CON", "PRN", "NUL", "AUX", NULL
};
static const char* fatReservedNames4[] = {
    "LPT1", "LPT2", "LPT3", "LPT4", "COM1", "COM2", "COM3", "COM4", NULL
};

/*
 * Filename normalization for Win32 filesystems.  You can't use [ \/:*?"<>| ].
 */
static NuError Win32NormalizeFileName(NulibState* pState, const char* srcp,
    long srcLen, char fssep, char** pDstp, long dstLen)
{
    char* dstp = *pDstp;
    const char* startp = srcp;
    static const char* kInvalid = "\\/:*?\"<>|";

    /* look for a match on "aux" or "aux\..*" */
    if (srcLen >= 3) {
        const char** ppcch;

        for (ppcch = fatReservedNames3; *ppcch != NULL; ppcch++) {
            if (strncasecmp(srcp, *ppcch, 3) == 0 &&
                srcp[3] == '.' || srcLen == 3)
            {
                DBUG(("--- fixing '%s'\n", *ppcch));
                if (NState_GetModPreserveType(pState)) {
                    *dstp++ = kForeignIndic;
                    *dstp++ = '0';
                    *dstp++ = '0';
                } else
                    *dstp++ = '_';
                break;
            }
        }
    }
    if (srcLen >= 4) {
        const char** ppcch;

        for (ppcch = fatReservedNames4; *ppcch != NULL; ppcch++) {
            if (strncasecmp(srcp, *ppcch, 4) == 0 &&
                srcp[4] == '.' || srcLen == 4)
            {
                DBUG(("--- fixing '%s'\n", *ppcch));
                if (NState_GetModPreserveType(pState)) {
                    *dstp++ = kForeignIndic;
                    *dstp++ = '0';
                    *dstp++ = '0';
                } else
                    *dstp++ = '_';
                break;
            }
        }
    }


    while (srcLen--) {      /* don't go until null found! */
        Assert(*srcp != '\0');

        if (*srcp == kForeignIndic) {
            /* change '%' to "%%" */
            if (NState_GetModPreserveType(pState))
                *dstp++ = *srcp;
            *dstp++ = *srcp++;
        } else if (strchr(kInvalid, *srcp) != NULL) {
            /* change invalid char to "%2f" or '_' */
            if (NState_GetModPreserveType(pState)) {
                *dstp++ = kForeignIndic;
                *dstp++ = HexConv(*srcp >> 4 & 0x0f);
                *dstp++ = HexConv(*srcp & 0x0f);
            } else {
                *dstp++ = '_';
            }
            srcp++;
        } else {
            /* no need to fiddle with it */
            *dstp++ = *srcp++;
        }
    }

    *dstp = '\0';       /* end the string, but don't advance past the null */
    Assert(*pDstp - dstp <= dstLen);    /* make sure we didn't overflow */
    *pDstp = dstp;

    return kNuErrNone;
}
#endif


/*
 * Normalize a file name to local filesystem conventions.  The input
 * is quite possibly *NOT* null-terminated, since it may represent a
 * substring of a full pathname.  Use "srcLen".
 *
 * The output filename is copied to *pDstp, which is advanced forward.
 *
 * The output buffer must be able to hold 3x the original string length.
 */
NuError NormalizeFileName(NulibState* pState, const char* srcp, long srcLen,
    char fssep, char** pDstp, long dstLen)
{
    NuError err;

    Assert(srcp != NULL);
    Assert(srcLen > 0);
    Assert(dstLen > srcLen);
    Assert(pDstp != NULL);
    Assert(*pDstp != NULL);
    Assert(fssep > ' ' && fssep < 0x7f);

#if defined(UNIX_LIKE)
    err = UNIXNormalizeFileName(pState, srcp, srcLen, fssep, pDstp, dstLen);
#elif defined(WINDOWS_LIKE)
    err = Win32NormalizeFileName(pState, srcp, srcLen, fssep, pDstp, dstLen);
#else
    #error "port this"
#endif

    return err;
}


/*
 * Normalize a directory name to local filesystem conventions.
 */
NuError NormalizeDirectoryName(NulibState* pState, const char* srcp,
    long srcLen, char fssep, char** pDstp, long dstLen)
{
    /* in general, directories and filenames are the same */
    return NormalizeFileName(pState, srcp, srcLen, fssep, pDstp, dstLen);
}


/*
 * Given the archive filename and the file system separator, strip off the
 * archive filename and replace it with the name of a nonexistent file
 * in the same directory.
 *
 * Under UNIX we just need the file to be on the same filesystem, but
 * under GS/OS it has to be in the same directory.  Not sure what Mac OS
 * or Windows requires, so it's safest to just put it in the same dir.
 */
char* MakeTempArchiveName(NulibState* pState)
{
    const char* archivePathname;
    char fssep;
    const char* nameStart;
    char* newName = NULL;
    char* namePtr;
    char* resultName = NULL;
    long len;

    archivePathname = NState_GetArchiveFilename(pState);
    Assert(archivePathname != NULL);
    fssep = NState_GetSystemPathSeparator(pState);
    Assert(fssep != 0);

    /* we'll get confused if the archive pathname looks like "/foo/bar/" */
    len = strlen(archivePathname);
    if (len < 1)
        goto bail;
    if (archivePathname[len-1] == fssep) {
        ReportError(kNuErrNone, "archive pathname can't end in '%c'", fssep);
        goto bail;
    }

    /* figure out where the filename ends */
    nameStart = strrchr(archivePathname, fssep);
    if (nameStart == NULL) {
        /* nothing but a filename */
        newName = Malloc(kTempFileNameLen +1);
        namePtr = newName;
    } else {
        nameStart++;    /* advance past the fssep */
        newName = Malloc((nameStart - archivePathname) + kTempFileNameLen +1);
        strcpy(newName, archivePathname);
        namePtr = newName + (nameStart - archivePathname);
    }
    if (newName == NULL)
        goto bail;

    /*
     * Create a new name with a mktemp-style template.
     */
    strcpy(namePtr, "nulibtmpXXXXXX");

    resultName = newName;

bail:
    if (resultName == NULL)
        Free(newName);
    return resultName;
}


/*
 * ===========================================================================
 *      Add a set of files
 * ===========================================================================
 */

/*
 * AddFile() and supporting functions.
 *
 * When adding one or more files, we need to add the file's attributes too,
 * including file type and access permissions.  We may want to recurse
 * into subdirectories.
 *
 * Because UNIX and GS/OS have rather different schemes for scanning
 * directories, I'm defining the whole thing as system-dependent instead
 * of trying to put an OS-dependent callback inside an OS-independent
 * wrapper.  The GS/OS directory scanning mechanism does everything stat()
 * does, plus picks up file types, so AddDirectory will want to pass a
 * lot more stuff into AddFile than the UNIX version.  And the UNIX and
 * Windows versions need to make filetype assumptions based on filename
 * extensions.
 *
 * We could force GS/OS to do an opendir/readdir/stat sort of thing, and
 * pass around some file type info that doesn't really get set under
 * UNIX or Windows, but that would be slower and more clumsy.
 */


#if defined(UNIX_LIKE) || defined(WINDOWS_LIKE)
/*
 * Check a file's status.
 *
 * [ Someday we may want to modify this to handle symbolic links. ]
 */
NuError CheckFileStatus(const char* pathname, struct stat* psb,
    Boolean* pExists, Boolean* pIsReadable, Boolean* pIsDir)
{
    NuError err = kNuErrNone;
    int cc;

    Assert(pathname != NULL);
    Assert(pExists != NULL);
    Assert(pIsReadable != NULL);
    Assert(pIsDir != NULL);

    *pExists = true;
    *pIsReadable = true;
    *pIsDir = false;

    cc = stat(pathname, psb);
    if (cc) {
        if (errno == ENOENT)
            *pExists = false;
        else
            err = kNuErrFileStat;
        goto bail;
    }

    if (S_ISDIR(psb->st_mode))
        *pIsDir = true;

    /*
     * Test if we can read this file.  How do we do that?  The easy but slow
     * way is to call access(2), the harder way is to figure out
     * what user/group we are and compare the appropriate file mode.
     */
    if (access(pathname, R_OK) < 0)
        *pIsReadable = false;

bail:
    return err;
}
#endif

#if defined(UNIX_LIKE) || defined(WINDOWS_LIKE)
/*
 * Convert from time in seconds to DateTime format.
 */
static void UNIXTimeToDateTime(const time_t* pWhen, NuDateTime *pDateTime)
{
    struct tm* ptm;

    Assert(pWhen != NULL);
    Assert(pDateTime != NULL);

    ptm = localtime(pWhen);
    pDateTime->second = ptm->tm_sec;
    pDateTime->minute = ptm->tm_min;
    pDateTime->hour = ptm->tm_hour;
    pDateTime->day = ptm->tm_mday -1;
    pDateTime->month = ptm->tm_mon;
    pDateTime->year = ptm->tm_year;
    pDateTime->extra = 0;
    pDateTime->weekDay = ptm->tm_wday +1;
}
#endif

#if defined(MAC_LIKE)
/*
 * Due to historical reasons, the XATTR_FINDERINFO_NAME (defined to be
 * ``com.apple.FinderInfo'') extended attribute must be 32 bytes; see the
 * ATTR_CMN_FNDRINFO section in getattrlist(2).
 *
 * The FinderInfo block is the concatenation of a FileInfo structure
 * and an ExtendedFileInfo (or ExtendedFolderInfo) structure -- see
 * ATTR_CMN_FNDRINFO in getattrlist(2).
 *
 * All we're really interested in is the file type and creator code,
 * which are stored big-endian in the first 8 bytes.
 */
static const int kFinderInfoSize = 32;

/*
 * Obtains the creator and file type from the Finder info block, if any,
 * and converts the types to ProDOS equivalents.
 *
 * If the attribute doesn't exist, this returns an error without modifying
 * the output args.
 */
static NuError GetTypesFromFinder(const char* pathnameUNI, uint32_t* pFileType,
    uint32_t* pAuxType)
{
    uint8_t fiBuf[kFinderInfoSize];

    size_t actual = getxattr(pathnameUNI, XATTR_FINDERINFO_NAME,
        fiBuf, sizeof(fiBuf), 0, 0);
    if (actual != kFinderInfoSize) {
        return kNuErrNotFound;
    }

    uint32_t fileType, creator;
    fileType = (fiBuf[0] << 24) | (fiBuf[1] << 16) | (fiBuf[2] << 8) |
        fiBuf[3];
    creator = (fiBuf[4] << 24) | (fiBuf[5] << 16) | (fiBuf[6] << 8) |
        fiBuf[7];

    Boolean found = false;
    uint8_t proType;
    uint16_t proAux;

    /*
     * Convert to ProDOS file/aux type.
     */
    if (creator == 'pdos') {
        /*
         * TODO: handle conversion from 'pdos'/'XY  ' to $XY/$0000.
         * I think this conversion was deprecated and not widely used;
         * the inability to retain the aux type renders it inapplicable
         * to many files.
         */
        if (fileType == 'PSYS') {
            proType = 0xFF;         // SYS
            proAux = 0x0000;
            found = true;
        } else if (fileType == 'PS16') {
            proType = 0xB3;         // S16
            proAux = 0x0000;
            found = true;
        } else {
            if (((fileType >> 24) & 0xFF) == 'p') {
                proType = (fileType >> 16) & 0xFF;
                proAux = (uint16_t) fileType;
                found = true;
            }
        }
    } else if (creator == 'dCpy') {
        if (fileType == 'dImg') {
            proType = 0xE0;         // LBR
            proAux = 0x0005;
            found = true;
        }
    }
    if (!found) {
        switch (fileType) {
            case 'BINA':
                proType = 0x06;     // BIN
                proAux = 0x0000;
                break;
            case 'TEXT':
                proType = 0x04;     // TXT
                proAux = 0x0000;
                break;
            case 'MIDI':
                proType = 0xD7;     // MDI
                proAux = 0x0000;
                break;
            case 'AIFF':
                proType = 0xD8;     // SND
                proAux = 0x0000;
                break;
            case 'AIFC':
                proType = 0xD8;     // SND
                proAux = 0x0001;
                break;
            default:
                proType = 0x00;     // NON
                proAux = 0x0000;
                break;
        }
    }

    *pFileType = proType;
    *pAuxType = proAux;
    return kNuErrNone;
}

/*
 * Set the file type and creator type, based on the ProDOS file type
 * and aux type.
 *
 * This is a clone of the function in NufxLib; it exists for the
 * benefit of the Binary ][ code.
 */
NuError SetFinderInfo(int fd, uint8_t proType, uint16_t proAux)
{
    uint8_t fiBuf[kFinderInfoSize];

    size_t actual = fgetxattr(fd, XATTR_FINDERINFO_NAME,
            fiBuf, sizeof(fiBuf), 0, 0);
    if (actual == (size_t) -1 && errno == ENOATTR) {
        // doesn't yet have Finder info
        memset(fiBuf, 0, sizeof(fiBuf));
    } else if (actual != kFinderInfoSize) {
        return kNuErrFile;
    }

    /*
     * Attempt to use one of the convenience types.  If nothing matches,
     * use the generic pdos/pXYZ approach.  Note that PSYS/PS16 will
     * lose the file's aux type.
     *
     * I'm told this is from page 336 of _Programmer's Reference for
     * System 6.0_.
     */
    uint8_t* fileTypeBuf = fiBuf;
    uint8_t* creatorBuf = fiBuf + 4;

    memcpy(creatorBuf, "pdos", 4);
    if (proType == 0x00 && proAux == 0x0000) {
        memcpy(fileTypeBuf, "BINA", 4);
    } else if (proType == 0x04 && proAux == 0x0000) {
        memcpy(fileTypeBuf, "TEXT", 4);
    } else if (proType == 0xff) {
        memcpy(fileTypeBuf, "PSYS", 4);
    } else if (proType == 0xb3 && (proAux & 0xff00) != 0xdb00) {
        memcpy(fileTypeBuf, "PS16", 4);
    } else if (proType == 0xd7 && proAux == 0x0000) {
        memcpy(fileTypeBuf, "MIDI", 4);
    } else if (proType == 0xd8 && proAux == 0x0000) {
        memcpy(fileTypeBuf, "AIFF", 4);
    } else if (proType == 0xd8 && proAux == 0x0001) {
        memcpy(fileTypeBuf, "AIFC", 4);
    } else if (proType == 0xe0 && proAux == 0x0005) {
        memcpy(creatorBuf, "dCpy", 4);
        memcpy(fileTypeBuf, "dImg", 4);
    } else {
        fileTypeBuf[0] = 'p';
        fileTypeBuf[1] = proType;
        fileTypeBuf[2] = (uint8_t) (proAux >> 8);
        fileTypeBuf[3] = (uint8_t) proAux;
    }

    if (fsetxattr(fd, XATTR_FINDERINFO_NAME, fiBuf, sizeof(fiBuf),
        0, 0) != 0)
    {
        return kNuErrFile;
    }

    return kNuErrNone;
}
#endif /*MAC_LIKE*/

#if defined(UNIX_LIKE) || defined(WINDOWS_LIKE)
/*
 * Replace "oldc" with "newc".  If we find an instance of "newc" already
 * in the string, replace it with "newSubst".
 */
static void ReplaceFssep(char* str, char oldc, char newc, char newSubst)
{
    while (*str != '\0') {
        if (*str == oldc)
            *str = newc;
        else if (*str == newc)
            *str = newSubst;
        str++;
    }
}

/*
 * Set the contents of a NuFileDetails structure, based on the pathname
 * and characteristics of the file.
 */
static NuError GetFileDetails(NulibState* pState, const char* pathnameMOR,
    struct stat* psb, NuFileDetails* pDetails)
{
    Boolean wasPreserved;
    Boolean doJunk = false;
    Boolean adjusted;
    char* livePathStr;
    char slashDotDotSlash[5] = "_.._";
    time_t now;

    Assert(pState != NULL);
    Assert(pathnameMOR != NULL);
    Assert(pDetails != NULL);

    /* set up the pathname buffer; note pDetails->storageName is const */
    NState_SetTempPathnameLen(pState, strlen(pathnameMOR) +1);
    livePathStr = NState_GetTempPathnameBuf(pState);
    Assert(livePathStr != NULL);
    strcpy(livePathStr, pathnameMOR);

    /* under Win32, both '/' and '\' work... we want to settle on one */
    if (NState_GetAltSystemPathSeparator(pState) != '\0') {
        ReplaceFssep(livePathStr,
            NState_GetAltSystemPathSeparator(pState),
            NState_GetSystemPathSeparator(pState),
            NState_GetSystemPathSeparator(pState));
    }

    /* init to defaults */
    memset(pDetails, 0, sizeof(*pDetails));
    pDetails->threadID = kNuThreadIDDataFork;
    pDetails->storageNameMOR = livePathStr;    /* point at temp buffer */
    pDetails->origName = NULL;
    pDetails->fileSysID = kNuFileSysUnknown;
    pDetails->fileSysInfo = kStorageFssep;
    pDetails->fileType = 0;
    pDetails->extraType = 0;
    pDetails->storageType = kNuStorageUnknown;  /* let NufxLib worry about it */
    if (psb->st_mode & S_IWUSR)
        pDetails->access = kNuAccessUnlocked;
    else
        pDetails->access = kNuAccessLocked;

    /* if this is a disk image, fill in disk-specific fields */
    if (NState_GetModAddAsDisk(pState)) {
        if ((psb->st_size & 0x1ff) != 0) {
            /* reject anything whose size isn't a multiple of 512 bytes */
            printf("NOT storing odd-sized (%ld) file as disk image: %s\n",
                (long)psb->st_size, livePathStr);
        } else {
            /* set fields; note the "preserve" stuff will override this */
            pDetails->threadID = kNuThreadIDDiskImage;
            pDetails->storageType = 512;
            pDetails->extraType = psb->st_size / 512;
        }
    }

    now = time(NULL);
    UNIXTimeToDateTime(&now, &pDetails->archiveWhen);
    UNIXTimeToDateTime(&psb->st_mtime, &pDetails->modWhen);
    UNIXTimeToDateTime(&psb->st_mtime, &pDetails->createWhen);

#ifdef MAC_LIKE
    /*
     * Retrieve the file/aux type from the Finder info.  We want the
     * type-preservation string to take priority, so get this first.
     */
    (void) GetTypesFromFinder(livePathStr,
            &pDetails->fileType, &pDetails->extraType);
#endif

    /*
     * Check for file type preservation info in the filename.  If present,
     * set the file type values and truncate the filename.
     */
    wasPreserved = false;
    if (NState_GetModPreserveType(pState)) {
        wasPreserved = ExtractPreservationString(pState, livePathStr,
                        &pDetails->fileType, &pDetails->extraType,
                        &pDetails->threadID);
    }

    /*
     * Do a "denormalization" pass, where we convert invalid chars (such
     * as '/') from percent-codes back to 8-bit characters.  The filename
     * will always be the same size or smaller, so we can do it in place.
     */
    if (wasPreserved)
        DenormalizePath(pState, livePathStr);

    /*
     * If we're in "extended" mode, and the file wasn't preserved, take a
     * guess at what the file type should be based on the file extension.
     */
    if (!wasPreserved && NState_GetModPreserveTypeExtended(pState)) {
        InterpretExtension(pState, livePathStr, &pDetails->fileType,
            &pDetails->extraType);
    }

    /*
     * Strip bad chars off the front of the pathname.  Every time we
     * remove one thing we potentially expose another, so we have to
     * loop until it's sanitized.
     *
     * The outer loop isn't really necessary under Win32, because you'd
     * need to do something like ".\\foo", which isn't allowed.  UNIX
     * silently allows ".//foo", so this is a problem there.  (We could
     * probably do away with the inner loops, but those were already
     * written when I saw the larger problem.)
     */
    do {
        adjusted = false;

        /*
         * Check for other unpleasantness, such as a leading fssep.
         */
        Assert(NState_GetSystemPathSeparator(pState) != '\0');
        while (livePathStr[0] == NState_GetSystemPathSeparator(pState)) {
            /* slide it down, len is (strlen +1), -1 (dropping first char)*/
            memmove(livePathStr, livePathStr+1, strlen(livePathStr));
            adjusted = true;
        }

        /*
         * Remove leading "./".
         */
        while (livePathStr[0] == '.' &&
            livePathStr[1] == NState_GetSystemPathSeparator(pState))
        {
            /* slide it down, len is (strlen +1) -2 (dropping two chars) */
            memmove(livePathStr, livePathStr+2, strlen(livePathStr)-1);
            adjusted = true;
        }
    } while (adjusted);

    /*
     * If there's a "/../" present anywhere in the name, junk everything
     * but the filename.
     *
     * This won't catch "foo/bar/..", but that should've been caught as
     * a directory anyway.
     */
    slashDotDotSlash[0] = NState_GetSystemPathSeparator(pState);
    slashDotDotSlash[3] = NState_GetSystemPathSeparator(pState);
    if ((livePathStr[0] == '.' && livePathStr[1] == '.') ||
        (strstr(livePathStr, slashDotDotSlash) != NULL))
    {
        DBUG(("Found dot dot in '%s', keeping only filename\n", livePathStr));
        doJunk = true;
    }

    /*
     * Scan for and remove "/./" and trailing "/.".  They're filesystem
     * no-ops that work just fine under Win32 and UNIX but could confuse
     * a IIgs.  (Of course, the user could just omit them from the pathname.)
     */
    /* TO DO 20030208 */

    /*
     * If "junk paths" is set, drop everything before the last fssep char.
     */
    if (NState_GetModJunkPaths(pState) || doJunk) {
        char* lastFssep;
        lastFssep = strrchr(livePathStr, NState_GetSystemPathSeparator(pState));
        if (lastFssep != NULL) {
            Assert(*(lastFssep+1) != '\0'); /* should already have been caught*/
            memmove(livePathStr, lastFssep+1, strlen(lastFssep+1)+1);
        }
    }

    /*
     * Finally, substitute our generally-accepted path separator in place of
     * the local one, stomping on anything with a ':' in it as we do.  The
     * goal is to avoid having "subdir:foo/bar" turn into "subdir/foo/bar".
     * Were we a general-purpose archiver, this might be a mistake, but
     * we're not.  NuFX doesn't really give us a choice.
     */
    ReplaceFssep(livePathStr, NState_GetSystemPathSeparator(pState),
        kStorageFssep, 'X');

/*bail:*/
    return kNuErrNone;
}
#endif


/*
 * Do the system-independent part of the file add, including things like
 * adding comments.
 */
static NuError DoAddFile(NulibState* pState, NuArchive* pArchive,
    const char* pathname, const NuFileDetails* pDetails)
{
    NuError err;
    NuRecordIdx recordIdx = 0;

    err = NuAddFile(pArchive, pathname, pDetails, false, &recordIdx);

    if (err == kNuErrNone) {
        NState_IncMatchCount(pState);
    } else if (err == kNuErrSkipped) {
        /* "maybe overwrite" UI causes this if user declines */
        err = kNuErrNone;
        goto bail;
    } else if (err == kNuErrNotNewer) {
        /* if we were expecting this, it's okay */
        if (NState_GetModFreshen(pState) || NState_GetModUpdate(pState)) {
            printf("SKIP older file: %s\n", pathname);
            err = kNuErrNone;
            goto bail;
        }
    } else if (err == kNuErrDuplicateNotFound) {
        /* if we were expecting this, it's okay */
        if (NState_GetModFreshen(pState)) {
            printf("SKIP file not in archive: %s\n", pathname);
            err = kNuErrNone;
            goto bail;
        }
    } else if (err == kNuErrRecordExists) {
        printf("FAIL same filename added twice: '%s'\n",
            NState_GetTempPathnameBuf(pState));
        goto bail_quiet;
    }
    if (err != kNuErrNone)
        goto bail;

    /* add a one-line comment if requested */
    if (NState_GetModComments(pState)) {
        char* comment;

        DBUG(("Preparing comment for recordIdx=%ld\n", recordIdx));
        Assert(recordIdx != 0);
        comment = GetSimpleComment(pState, pathname, kDefaultCommentLen);
        if (comment != NULL) {
            NuDataSource* pDataSource;

            err = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
                    kDefaultCommentLen, (uint8_t*)comment, 0,
                    strlen(comment), FreeCallback, &pDataSource);
            if (err != kNuErrNone) {
                ReportError(err, "comment buffer create failed");
                Free(comment);
                err = kNuErrNone;   /* oh well */
            } else {
                comment = NULL;  /* now owned by the data source */
                err = NuAddThread(pArchive, recordIdx, kNuThreadIDComment,
                        pDataSource, NULL);
                if (err != kNuErrNone) {
                    ReportError(err, "comment thread add failed");
                    NuFreeDataSource(pDataSource);
                    err = kNuErrNone;   /* oh well */
                } else {
                    pDataSource = NULL;  /* now owned by NufxLib */
                }
            }
        }
    }

bail:
    if (err != kNuErrNone)
        ReportError(err, "unable to add file");
bail_quiet:
    return err;
}


#if defined(UNIX_LIKE)  /* ---- UNIX --------------------------------------- */
static NuError UNIXAddFile(NulibState* pState, NuArchive* pArchive,
    const char* pathname);

/*
 * UNIX-style recursive directory descent.  Scan the contents of a directory.
 * If a subdirectory is found, follow it; otherwise, call UNIXAddFile to
 * add the file.
 */
static NuError UNIXAddDirectory(NulibState* pState, NuArchive* pArchive,
    const char* dirName)
{
    NuError err = kNuErrNone;
    DIR* dirp = NULL;
    DIR_TYPE* entry;
    char nbuf[MAX_PATH_LEN];    /* malloc might be better; this soaks stack */
    char fssep;
    int len;

    Assert(pState != NULL);
    Assert(pArchive != NULL);
    Assert(dirName != NULL);

    DBUG(("+++ DESCEND: '%s'\n", dirName));

    dirp = opendir(dirName);
    if (dirp == NULL) {
        if (errno == ENOTDIR)
            err = kNuErrNotDir;
        else
            err = errno ? errno : kNuErrOpenDir;
        ReportError(err, "failed on '%s'", dirName);
        goto bail;
    }

    fssep = NState_GetSystemPathSeparator(pState);

    /* could use readdir_r, but we don't care about reentrancy here */
    while ((entry = readdir(dirp)) != NULL) {
        /* skip the dotsies */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        len = strlen(dirName);
        if (len + DIR_NAME_LEN(entry) +2 > MAX_PATH_LEN) {
            err = kNuErrInternal;
            ReportError(err, "Filename exceeds %d bytes: %s%c%s",
                MAX_PATH_LEN, dirName, fssep, entry->d_name);
            goto bail;
        }

        /* form the new name, inserting an fssep if needed */
        strcpy(nbuf, dirName);
        if (dirName[len-1] != fssep)
            nbuf[len++] = fssep;
        strcpy(nbuf+len, entry->d_name);

        err = UNIXAddFile(pState, pArchive, nbuf);
        if (err != kNuErrNone)
            goto bail;
    }

bail:
    if (dirp != NULL)
        (void)closedir(dirp);
    return err;
}

/*
 * Add a file to the list we're adding to the archive.
 *
 * If the file is a directory, and we allow recursing into subdirectories,
 * this calls UNIXAddDirectory.  If we don't allow recursion, this just
 * returns without an error.
 *
 * Returns with an error if the file doesn't exist or isn't readable.
 */
static NuError UNIXAddFile(NulibState* pState, NuArchive* pArchive,
    const char* pathname)
{
    NuError err = kNuErrNone;
    Boolean exists, isDir, isReadable;
    NuFileDetails details;
    struct stat sb;

    Assert(pState != NULL);
    Assert(pArchive != NULL);
    Assert(pathname != NULL);

    err = CheckFileStatus(pathname, &sb, &exists, &isReadable, &isDir);
    if (err != kNuErrNone) {
        ReportError(err, "unexpected error while examining '%s'", pathname);
        goto bail;
    }

    if (!exists) {
        err = kNuErrFileNotFound;
        ReportError(err, "couldn't find '%s'", pathname);
        goto bail;
    }
    if (!isReadable) {
        ReportError(kNuErrNone, "file '%s' isn't readable", pathname);
        err = kNuErrFileNotReadable;
        goto bail;
    }
    if (isDir) {
        if (NState_GetModRecurse(pState))
            err = UNIXAddDirectory(pState, pArchive, pathname);
        goto bail_quiet;
    }

    /*
     * We've found a file that we want to add.  We need to decide what
     * filetype and auxtype it has, and whether or not it's actually the
     * resource fork of another file.
     */
    DBUG(("+++ ADD '%s'\n", pathname));

    err = GetFileDetails(pState, pathname, &sb, &details);
    if (err != kNuErrNone)
        goto bail;

    err = DoAddFile(pState, pArchive, pathname, &details);
    if (err != kNuErrNone)
        goto bail_quiet;

bail:
    if (err != kNuErrNone)
        ReportError(err, "unable to add file");
bail_quiet:
    return err;
}

#elif defined(WINDOWS_LIKE) /* ---- Windows -------------------------------- */

/*
 * Directory structure and functions, based on zDIR in Info-Zip sources.
 */
typedef struct Win32dirent {
    char    d_attr;
    char    d_name[MAX_PATH_LEN];
    int     d_first;
    HANDLE  d_hFindFile;
} Win32dirent;

static const char* kWildMatchAll = "*.*";

/*
 * Prepare a directory for reading.
 */
static Win32dirent* OpenDir(const char* name)
{
    Win32dirent* dir = NULL;
    char* tmpStr = NULL;
    char* cp;
    WIN32_FIND_DATA fnd;

    dir = Malloc(sizeof(*dir));
    tmpStr = Malloc(strlen(name) + (2 + sizeof(kWildMatchAll)));
    if (dir == NULL || tmpStr == NULL)
        goto failed;

    strcpy(tmpStr, name);
    cp = tmpStr + strlen(tmpStr);

    /* don't end in a colon (e.g. "C:") */
    if ((cp - tmpStr) > 0 && strrchr(tmpStr, ':') == (cp - 1))
        *cp++ = '.';
    /* must end in a slash */
    if ((cp - tmpStr) > 0 && strrchr(tmpStr, PATH_SEP) != (cp - 1))
        *cp++ = PATH_SEP;

    strcpy(cp, kWildMatchAll);

    dir->d_hFindFile = FindFirstFile(tmpStr, &fnd);
    if (dir->d_hFindFile == INVALID_HANDLE_VALUE)
        goto failed;

    strcpy(dir->d_name, fnd.cFileName);
    dir->d_attr = (uint8_t) fnd.dwFileAttributes;
    dir->d_first = 1;

bail:
    Free(tmpStr);
    return dir;

failed:
    Free(dir);
    dir = NULL;
    goto bail;
}

/*
 * Get an entry from an open directory.
 *
 * Returns a NULL pointer after the last entry has been read.
 */
static Win32dirent* ReadDir(Win32dirent* dir)
{
    if (dir->d_first)
        dir->d_first = 0;
    else {
        WIN32_FIND_DATA fnd;

        if (!FindNextFile(dir->d_hFindFile, &fnd))
            return NULL;
        strcpy(dir->d_name, fnd.cFileName);
        dir->d_attr = (uint8_t) fnd.dwFileAttributes;
    }

    return dir;
}

/*
 * Close a directory.
 */
static void CloseDir(Win32dirent* dir)
{
    if (dir == NULL)
        return;

    FindClose(dir->d_hFindFile);
    Free(dir);
}


/* might as well blend in with the UNIX version */
#define DIR_NAME_LEN(dirent)    ((int)strlen((dirent)->d_name))

static NuError Win32AddFile(NulibState* pState, NuArchive* pArchive,
    const char* pathname);


/*
 * Win32 recursive directory descent.  Scan the contents of a directory.
 * If a subdirectory is found, follow it; otherwise, call Win32AddFile to
 * add the file.
 */
static NuError Win32AddDirectory(NulibState* pState, NuArchive* pArchive,
    const char* dirName)
{
    NuError err = kNuErrNone;
    Win32dirent* dirp = NULL;
    Win32dirent* entry;
    char nbuf[MAX_PATH_LEN];    /* malloc might be better; this soaks stack */
    char fssep;
    int len;

    Assert(pState != NULL);
    Assert(pArchive != NULL);
    Assert(dirName != NULL);

    DBUG(("+++ DESCEND: '%s'\n", dirName));

    dirp = OpenDir(dirName);
    if (dirp == NULL) {
        if (errno == ENOTDIR)
            err = kNuErrNotDir;
        else
            err = errno ? errno : kNuErrOpenDir;
        ReportError(err, "failed on '%s'", dirName);
        goto bail;
    }

    fssep = NState_GetSystemPathSeparator(pState);

    /* could use readdir_r, but we don't care about reentrancy here */
    while ((entry = ReadDir(dirp)) != NULL) {
        /* skip the dotsies */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        len = strlen(dirName);
        if (len + DIR_NAME_LEN(entry) +2 > MAX_PATH_LEN) {
            err = kNuErrInternal;
            ReportError(err, "Filename exceeds %d bytes: %s%c%s",
                MAX_PATH_LEN, dirName, fssep, entry->d_name);
            goto bail;
        }

        /* form the new name, inserting an fssep if needed */
        strcpy(nbuf, dirName);
        if (dirName[len-1] != fssep)
            nbuf[len++] = fssep;
        strcpy(nbuf+len, entry->d_name);

        err = Win32AddFile(pState, pArchive, nbuf);
        if (err != kNuErrNone)
            goto bail;
    }

bail:
    if (dirp != NULL)
        (void)CloseDir(dirp);
    return err;
}

/*
 * Add a file to the list we're adding to the archive.  If it's a directory,
 * and the recursive descent feature is enabled, call Win32AddDirectory to
 * add the contents of the dir.
 *
 * Returns with an error if the file doesn't exist or isn't readable.
 */
static NuError Win32AddFile(NulibState* pState, NuArchive* pArchive,
    const char* pathname)
{
    NuError err = kNuErrNone;
    Boolean exists, isDir, isReadable;
    NuFileDetails details;
    struct stat sb;

    Assert(pState != NULL);
    Assert(pArchive != NULL);
    Assert(pathname != NULL);

    err = CheckFileStatus(pathname, &sb, &exists, &isReadable, &isDir);
    if (err != kNuErrNone) {
        ReportError(err, "unexpected error while examining '%s'", pathname);
        goto bail;
    }

    if (!exists) {
        err = kNuErrFileNotFound;
        ReportError(err, "couldn't find '%s'", pathname);
        goto bail;
    }
    if (!isReadable) {
        ReportError(kNuErrNone, "file '%s' isn't readable", pathname);
        err = kNuErrFileNotReadable;
        goto bail;
    }
    if (isDir) {
        if (NState_GetModRecurse(pState))
            err = Win32AddDirectory(pState, pArchive, pathname);
        goto bail_quiet;
    }

    /*
     * We've found a file that we want to add.  We need to decide what
     * filetype and auxtype it has, and whether or not it's actually the
     * resource fork of another file.
     */
    DBUG(("+++ ADD '%s'\n", pathname));

    err = GetFileDetails(pState, pathname, &sb, &details);
    if (err != kNuErrNone)
        goto bail;

    err = DoAddFile(pState, pArchive, pathname, &details);
    if (err != kNuErrNone)
        goto bail_quiet;

bail:
    if (err != kNuErrNone)
        ReportError(err, "unable to add file");
bail_quiet:
    return err;
}

#else  /* ---- unknown ----------------------------------------------------- */
# error "Port this (AddFile/AddDirectory)"
#endif


/*
 * External entry point; just calls the system-specific version.
 *
 * [ I figure the GS/OS version will want to pass a copy of the file
 *   info from the GSOSAddDirectory function back into GSOSAddFile, so we'd
 *   want to call it from here with a NULL pointer indicating that we
 *   don't yet have the file info.  That way we can get the file info
 *   from the directory read call and won't have to check it again in
 *   GSOSAddFile. ]
 */
NuError AddFile(NulibState* pState, NuArchive* pArchive, const char* pathname)
{
#if defined(UNIX_LIKE)
    return UNIXAddFile(pState, pArchive, pathname);
#elif defined(WINDOWS_LIKE)
    return Win32AddFile(pState, pArchive, pathname);
#else
    #error "Port this"
#endif
}


/*
 * Invoke the system-dependent directory creation function.
 *
 * Currently only used by Binary2.c.
 */
NuError Mkdir(const char* dir)
{
    NuError err = kNuErrNone;

    Assert(dir != NULL);

#if defined(UNIX_LIKE)
    if (mkdir(dir, S_IRWXU | S_IRGRP|S_IXGRP | S_IROTH|S_IXOTH) < 0) {
        err = errno ? errno : kNuErrDirCreate;
        goto bail;
    }

#elif defined(WINDOWS_LIKE)
    if (mkdir(dir) < 0) {
        err = errno ? errno : kNuErrDirCreate;
        goto bail;
    }

#else
    #error "Port this"
#endif

bail:
    return err;
}

/*
 * Test for the existence of a file.
 *
 * Currently only used by Binary2.c.
 */
NuError TestFileExistence(const char* fileName, Boolean* pIsDir)
{
    NuError err = kNuErrNone;
    Assert(fileName != NULL);
    Assert(pIsDir != NULL);

#if defined(UNIX_LIKE) || defined(WINDOWS_LIKE)
    {
        struct stat sbuf;
        int cc;

        cc = stat(fileName, &sbuf);
        if (cc) {
            if (errno == ENOENT)
                err = kNuErrFileNotFound;
            else
                err = kNuErrFileStat;
            goto bail;
        }

        if (S_ISDIR(sbuf.st_mode))
            *pIsDir = true;
        else
            *pIsDir = false;
    }
#else
    #error "Port this"
#endif

bail:
    return err;
}

