/*
 * Nulib2
 * Copyright (C) 2000 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 *
 * System-dependent utility functions.
 */
#include "Nulib2.h"

#ifdef HAVE_WINDOWS_H
# include <windows.h>
#endif

/* get a grip on this opendir/readdir stuff */
#if defined(UNIX_LIKE)
#  if defined(HAVE_DIRENT_H)
#    include <dirent.h>
#    define DIR_NAME_LEN(dirent)	((int)strlen((dirent)->d_name))
     typedef struct dirent DIR_TYPE;
#  elif defined(HAVE_SYS_DIR_H)
#    include <sys/dir.h>
#    define DIR_NAME_LEN(direct)	((direct)->d_namlen)
     typedef struct direct DIR_TYPE;
#  elif defined(HAVE_NDIR_H)
#    include <sys/ndir.h>
#    define DIR_NAME_LEN(direct)	((direct)->d_namlen)
     typedef struct direct DIR_TYPE;
#  else
#    error "Port this?"
#  endif
#endif

/*
 * For systems (e.g. Visual C++ 6.0) that don't have these standard values.
 */
#ifndef S_IRUSR
# define S_IRUSR	0400
# define S_IWUSR	0200
# define S_IXUSR	0100
# define S_IRWXU	(S_IRUSR|S_IWUSR|S_IXUSR)
# define S_IRGRP	(S_IRUSR >> 3)
# define S_IWGRP	(S_IWUSR >> 3)
# define S_IXGRP	(S_IXUSR >> 3)
# define S_IRWXG	(S_IRWXU >> 3)
# define S_IROTH	(S_IRGRP >> 3)
# define S_IWOTH	(S_IWGRP >> 3)
# define S_IXOTH	(S_IXGRP >> 3)
# define S_IRWXO	(S_IRWXG >> 3)
#endif
#ifndef S_ISREG
# define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
# define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#endif


/*
 * ===========================================================================
 *		System-specific filename stuff
 * ===========================================================================
 */

#define kTempFileNameLen	20


#if defined(UNIX_LIKE)

/*
 * Filename normalization for typical UNIX filesystems.  Only '/' is
 * forbidden.  Maximum filename length is large enough that we might
 * as well just let the filesystem truncate if it gets too long, rather
 * than worry about truncating it cleverly.
 */
static NuError
UNIXNormalizeFileName(NulibState* pState, const char* srcp, long srcLen,
	char fssep, char** pDstp, long dstLen)
{
	char* dstp = *pDstp;

	while (srcLen--) {		/* don't go until null found! */
		assert(*srcp != '\0');

		if (*srcp == '%') {
			/* change '%' to "%%" */
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

	*dstp = '\0';		/* end the string, but don't advance past the null */
	assert(*pDstp - dstp <= dstLen);	/* make sure we didn't overflow */
	*pDstp = dstp;

	return kNuErrNone;
}

#elif defined(WINDOWS_LIKE)
/*
 * You can't create files or directories with these names on a FAT filesystem,
 * because they're MS-DOS "device special files".
 *
 * The list comes from the Linux kernel's fs/msdos/namei.c.
 */
static const char* fatReservedNames3[] = {
	"CON", "PRN", "NUL", "AUX", nil
};
static const char* fatReservedNames4[] = {
	"LPT1", "LPT2", "LPT3", "LPT4", "COM1", "COM2", "COM3", "COM4", nil
};

/*
 * Filename normalization for Win32 filesystems.  You can't use [ \/:*?"<>| ].
 */
static NuError
Win32NormalizeFileName(NulibState* pState, const char* srcp, long srcLen,
	char fssep, char** pDstp, long dstLen)
{
	char* dstp = *pDstp;
	const char* startp = srcp;
	static const char* kInvalid = "\\/:*?\"<>|";

	/* look for an exact match */
	if (srcLen == 3) {
		const char** ppcch;

		for (ppcch = fatReservedNames3; *ppcch != nil; ppcch++) {
			if (strncasecmp(srcp, *ppcch, srcLen) == 0) {
				DBUG(("--- fixing '%s'\n", *ppcch));
				*dstp++ = '_';
				break;
			}
		}
	} else if (srcLen == 4) {
		const char** ppcch;

		for (ppcch = fatReservedNames4; *ppcch != nil; ppcch++) {
			if (strncasecmp(srcp, *ppcch, srcLen) == 0) {
				DBUG(("--- fixing '%s'\n", *ppcch));
				*dstp++ = '_';
				break;
			}
		}
	}


	while (srcLen--) {		/* don't go until null found! */
		assert(*srcp != '\0');

		if (*srcp == '%') {
			/* change '%' to "%%" */
			*dstp++ = *srcp;
			*dstp++ = *srcp++;
		} else if (strchr(kInvalid, *srcp) != nil) {
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

	*dstp = '\0';		/* end the string, but don't advance past the null */
	assert(*pDstp - dstp <= dstLen);	/* make sure we didn't overflow */
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
NuError
NormalizeFileName(NulibState* pState, const char* srcp, long srcLen,
	char fssep, char** pDstp, long dstLen)
{
	NuError err;

	assert(srcp != nil);
	assert(srcLen > 0);
	assert(dstLen > srcLen);
	assert(pDstp != nil);
	assert(*pDstp != nil);
	assert(fssep > ' ' && fssep < 0x7f);

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
NuError
NormalizeDirectoryName(NulibState* pState, const char* srcp, long srcLen,
	char fssep, char** pDstp, long dstLen)
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
char*
MakeTempArchiveName(NulibState* pState)
{
	const char* archivePathname;
	char fssep;
	const char* nameStart;
	char* newName = nil;
	char* namePtr;
	char* resultName = nil;
	long len;

	archivePathname = NState_GetArchiveFilename(pState);
	assert(archivePathname != nil);
	fssep = NState_GetSystemPathSeparator(pState);
	assert(fssep != 0);

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
	if (nameStart == nil) {
		/* nothing but a filename */
		newName = Malloc(kTempFileNameLen +1);
		namePtr = newName;
	} else {
		nameStart++;	/* advance past the fssep */
		newName = Malloc((nameStart - archivePathname) + kTempFileNameLen +1);
		strcpy(newName, archivePathname);
		namePtr = newName + (nameStart - archivePathname);
	}
	if (newName == nil)
		goto bail;

	/*
	 * Create a new name with a mktemp-style template.
	 */
	strcpy(namePtr, "nulibtmpXXXXXX");

	resultName = newName;

bail:
	if (resultName == nil)
		Free(newName);
	return resultName;
}


/*
 * ===========================================================================
 *		Add a set of files
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
NuError
CheckFileStatus(const char* pathname, struct stat* psb, Boolean* pExists,
	Boolean* pIsReadable, Boolean* pIsDir)
{
	NuError err = kNuErrNone;
	int cc;

	assert(pathname != nil);
	assert(pExists != nil);
	assert(pIsReadable != nil);
	assert(pIsDir != nil);

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
static void
UNIXTimeToDateTime(const time_t* pWhen, NuDateTime *pDateTime)
{
	struct tm* ptm;

	Assert(pWhen != nil);
	Assert(pDateTime != nil);

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

#if defined(UNIX_LIKE) || defined(WINDOWS_LIKE)
/*
 * Set the contents of a NuFileDetails structure, based on the pathname
 * and characteristics of the file.
 */
static NuError
SetFileDetails(NulibState* pState, const char* pathname, struct stat* psb,
	NuFileDetails* pDetails)
{
	Boolean wasPreserved;
	Boolean doJunk = false;
	char* livePathStr;
	char slashDotDotSlash[5] = "_.._";
	time_t now;

	assert(pState != nil);
	assert(pathname != nil);
	assert(pDetails != nil);

	/* set up the pathname buffer; note pDetails->storageName is const */
	NState_SetTempPathnameLen(pState, strlen(pathname) +1);
	livePathStr = NState_GetTempPathnameBuf(pState);
	assert(livePathStr != nil);
	strcpy(livePathStr, pathname);

	/* init to defaults */
	memset(pDetails, 0, sizeof(*pDetails));
	pDetails->threadID = kNuThreadIDDataFork;
	pDetails->storageName = livePathStr;	/* point at temp buffer */
	pDetails->fileSysID = kNuFileSysUnknown;
	pDetails->fileSysInfo = NState_GetSystemPathSeparator(pState);
	pDetails->fileType = 0;
	pDetails->extraType = 0;
	pDetails->storageType = kNuStorageUnknown;	/* let NufxLib worry about it */
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

	now = time(nil);
	UNIXTimeToDateTime(&now, &pDetails->archiveWhen);
	UNIXTimeToDateTime(&psb->st_mtime, &pDetails->modWhen);
	UNIXTimeToDateTime(&psb->st_mtime, &pDetails->createWhen);

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
	 * Check for other unpleasantness, such as a leading fssep.
	 */
	assert(NState_GetSystemPathSeparator(pState) != '\0');
	while (livePathStr[0] == NState_GetSystemPathSeparator(pState)) {
		/* slide it down, len is strlen +1 (for null) -1 (dropping first char)*/
		memmove(livePathStr, livePathStr+1, strlen(livePathStr));
	}

	/*
	 * Remove leading "./".
	 */
	while (livePathStr[0] == '.' &&
		livePathStr[1] == NState_GetSystemPathSeparator(pState))
	{
		/* slide it down, len is strlen +1 (for null) -2 (dropping two chars) */
		memmove(livePathStr, livePathStr+2, strlen(livePathStr)-1);
	}

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
		(strstr(livePathStr, slashDotDotSlash) != nil))
	{
		DBUG(("Found dot dot in '%s', keeping only filename\n", livePathStr));
		doJunk = true;
	}

	/*
	 * If "junk paths" is set, drop everything before the last fssep char.
	 */
	if (NState_GetModJunkPaths(pState) || doJunk) {
		char* lastFssep;
		lastFssep = strrchr(livePathStr, NState_GetSystemPathSeparator(pState));
		if (lastFssep != nil) {
			assert(*(lastFssep+1) != '\0');	/* should already have been caught*/
			memmove(livePathStr, lastFssep+1, strlen(lastFssep+1)+1);
		}
	}

/*bail:*/
	return kNuErrNone;
}
#endif


/*
 * Do the system-independent part of the file add, including things like
 * adding comments.
 */
NuError
DoAddFile(NulibState* pState, NuArchive* pArchive, const char* pathname,
	const NuFileDetails* pDetails)
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
		assert(recordIdx != 0);
		comment = GetSimpleComment(pState, pathname, kDefaultCommentLen);
		if (comment != nil) {
			NuDataSource* pDataSource;

			err = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
					true, kDefaultCommentLen, (unsigned char*)comment, 0,
					strlen(comment), &pDataSource);
			if (err != kNuErrNone) {
				ReportError(err, "comment buffer create failed");
				Free(comment);
				err = kNuErrNone;	/* oh well */
			} else {
				comment = nil;	/* now owned by the data source */
				err = NuAddThread(pArchive, recordIdx, kNuThreadIDComment,
						pDataSource, nil);
				if (err != kNuErrNone) {
					ReportError(err, "comment thread add failed");
					NuFreeDataSource(pDataSource);
					err = kNuErrNone;	/* oh well */
				} else {
					pDataSource = nil;	/* now owned by NufxLib */
				}
			}
		}
	}

bail:
	if (err != kNuErrNone)
		ReportError(err, "Unable to add file");
bail_quiet:
	return err;
}


#if defined(UNIX_LIKE)
static NuError UNIXAddFile(NulibState* pState, NuArchive* pArchive,
	const char* pathname);

/*
 * UNIX-style recursive directory descent.  Scan the contents of a directory.
 * If a subdirectory is found, follow it; otherwise, call UNIXAddFile to
 * add the file.
 */
static NuError
UNIXAddDirectory(NulibState* pState, NuArchive* pArchive, const char* dirName)
{
	NuError err = kNuErrNone;
	DIR* dirp = nil;
	DIR_TYPE* entry;
	char nbuf[MAX_PATH_LEN];	/* malloc might be better; this soaks stack */
	char fssep;
	int len;

	assert(pState != nil);
	assert(pArchive != nil);
	assert(dirName != nil);

	DBUG(("+++ DESCEND: '%s'\n", dirName));

	dirp = opendir(dirName);
	if (dirp == nil) {
		if (errno == ENOTDIR)
			err = kNuErrNotDir;
		else
			err = errno ? errno : kNuErrOpenDir;
		ReportError(err, "failed on '%s'", dirName);
		goto bail;
	}

	fssep = NState_GetSystemPathSeparator(pState);

	/* could use readdir_r, but we don't care about reentrancy here */
	while ((entry = readdir(dirp)) != nil) {
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
	if (dirp != nil)
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
static NuError
UNIXAddFile(NulibState* pState, NuArchive* pArchive, const char* pathname)
{
	NuError err = kNuErrNone;
	Boolean exists, isDir, isReadable;
	NuFileDetails details;
	struct stat sb;

	assert(pState != nil);
	assert(pArchive != nil);
	assert(pathname != nil);

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

	err = SetFileDetails(pState, pathname, &sb, &details);
	if (err != kNuErrNone)
		goto bail;

	err = DoAddFile(pState, pArchive, pathname, &details);
	if (err != kNuErrNone)
		goto bail_quiet;

bail:
	if (err != kNuErrNone)
		ReportError(err, "Unable to add file");
bail_quiet:
	return err;
}

#elif defined(WINDOWS_LIKE)

/*
 * Directory structure and functions, based on zDIR in Info-Zip sources.
 */
typedef struct Win32dirent {
	char	d_attr;
	char	d_name[MAX_PATH_LEN];
	int		d_first;
	HANDLE	d_hFindFile;
} Win32dirent;

static const char* kWildMatchAll = "*.*";

/*
 * Prepare a directory for reading.
 */
static Win32dirent*
OpenDir(const char* name)
{
	Win32dirent* dir = nil;
	char* tmpStr = nil;
	char* cp;
	WIN32_FIND_DATA fnd;

	dir = Malloc(sizeof(*dir));
	tmpStr = Malloc(strlen(name) + (2 + sizeof(kWildMatchAll)));
	if (dir == nil || tmpStr == nil)
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
	dir->d_attr = (uchar) fnd.dwFileAttributes;
	dir->d_first = 1;

bail:
	Free(tmpStr);
	return dir;

failed:
	Free(dir);
	dir = nil;
	goto bail;
}

/*
 * Get an entry from an open directory.
 *
 * Returns a nil pointer after the last entry has been read.
 */
static Win32dirent*
ReadDir(Win32dirent* dir)
{
	if (dir->d_first)
		dir->d_first = 0;
	else {
		WIN32_FIND_DATA fnd;

		if (!FindNextFile(dir->d_hFindFile, &fnd))
			return nil;
		strcpy(dir->d_name, fnd.cFileName);
		dir->d_attr = (uchar) fnd.dwFileAttributes;
	}

	return dir;
}

/*
 * Close a directory.
 */
static void
CloseDir(Win32dirent* dir)
{
	if (dir == nil)
		return;

	FindClose(dir->d_hFindFile);
	Free(dir);
}


/* might as well blend in with the UNIX version */
#define DIR_NAME_LEN(dirent)	((int)strlen((dirent)->d_name))

static NuError Win32AddFile(NulibState* pState, NuArchive* pArchive,
	const char* pathname);


/*
 * Win32 recursive directory descent.  Scan the contents of a directory.
 * If a subdirectory is found, follow it; otherwise, call Win32AddFile to
 * add the file.
 */
static NuError
Win32AddDirectory(NulibState* pState, NuArchive* pArchive, const char* dirName)
{
	NuError err = kNuErrNone;
	Win32dirent* dirp = nil;
	Win32dirent* entry;
	char nbuf[MAX_PATH_LEN];	/* malloc might be better; this soaks stack */
	char fssep;
	int len;

	assert(pState != nil);
	assert(pArchive != nil);
	assert(dirName != nil);

	DBUG(("+++ DESCEND: '%s'\n", dirName));

	dirp = OpenDir(dirName);
	if (dirp == nil) {
		if (errno == ENOTDIR)
			err = kNuErrNotDir;
		else
			err = errno ? errno : kNuErrOpenDir;
		ReportError(err, "failed on '%s'", dirName);
		goto bail;
	}

	fssep = NState_GetSystemPathSeparator(pState);

	/* could use readdir_r, but we don't care about reentrancy here */
	while ((entry = ReadDir(dirp)) != nil) {
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
	if (dirp != nil)
		(void)CloseDir(dirp);
	return err;
}

/*
 * Add a file to the list we're adding to the archive.
 *
 * I haven't figured out the recursive tree traversal stuff yet, so for
 * now this ignores directories.
 *
 * Returns with an error if the file doesn't exist or isn't readable.
 */
static NuError
Win32AddFile(NulibState* pState, NuArchive* pArchive, const char* pathname)
{
	NuError err = kNuErrNone;
	Boolean exists, isDir, isReadable;
	NuFileDetails details;
	struct stat sb;

	assert(pState != nil);
	assert(pArchive != nil);
	assert(pathname != nil);

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

	err = SetFileDetails(pState, pathname, &sb, &details);
	if (err != kNuErrNone)
		goto bail;

	err = DoAddFile(pState, pArchive, pathname, &details);
	if (err != kNuErrNone)
		goto bail_quiet;

bail:
	if (err != kNuErrNone)
		ReportError(err, "Unable to add file");
bail_quiet:
	return err;
}

#else
# error "Port this (AddFile/AddDirectory)"
#endif


/*
 * External entry point; just calls the system-specific version.
 *
 * [ I figure the GS/OS version will want to pass a copy of the file
 *   info from the GSOSAddDirectory function back into GSOSAddFile, so we'd
 *   want to call it from here with a nil pointer indicating that we
 *   don't yet have the file info.  That way we can get the file info
 *   from the directory read call and won't have to check it again in
 *   GSOSAddFile. ]
 */
NuError
AddFile(NulibState* pState, NuArchive* pArchive, const char* pathname)
{
#if defined(UNIX_LIKE)
	return UNIXAddFile(pState, pArchive, pathname);
#elif defined(WINDOWS_LIKE)
	return Win32AddFile(pState, pArchive, pathname);
#else
	#error "Port this"
#endif
}

