/*
 * Nulib2
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.
 */
#ifndef __Nulib2__
#define __Nulib2__

#include "SysDefs.h"    /* system-dependent defs; must come first */
#include <NufxLib.h>
#include "State.h"
#include "MiscStuff.h"

#ifdef USE_DMALLOC
/* enable with something like "dmalloc -l logfile -i 100 medium" */
# include "dmalloc.h"
#endif

/* replace unsupported chars with '%xx' */
#define kForeignIndic       '%'

/* use this to separate path components in stored filenames */
#define kStorageFssep       ':'

/* make our one-line comments this big */
#define kDefaultCommentLen  200

/* for use with FormatDateShort() */
#define kDateOutputLen    64

/*
 * Function prototypes.
 */

/* Add.c */
NuError DoAdd(NulibState* pState);

/* ArcUtils.c */
char* GetSimpleComment(NulibState* pState, const char* pathname, int maxLen);
Boolean IsFilenameStdin(const char* archiveName);
Boolean IsSpecified(NulibState* pState, const NuRecord* pRecord);
NuError OpenArchiveReadOnly(NulibState* pState);
NuError OpenArchiveReadWrite(NulibState* pState);
const NuThread* GetThread(const NuRecord* pRecord, ulong idx);
Boolean IsRecordReadOnly(const NuRecord* pRecord);

/* Binary2.c */
NuError BNYDoExtract(NulibState* pState);
NuError BNYDoTest(NulibState* pState);
NuError BNYDoListShort(NulibState* pState);
NuError BNYDoListVerbose(NulibState* pState);
NuError BNYDoListDebug(NulibState* pState);

/* Delete.c */
NuError DoDelete(NulibState* pState);

/* Extract.c */
NuError DoExtract(NulibState* pState);
NuError DoExtractToPipe(NulibState* pState);
NuError DoTest(NulibState* pState);

/* Filename.c */
const char* GetFileTypeString(ulong fileType);
const char* NormalizePath(NulibState* pState, NuPathnameProposal* pathProposal);
void InterpretExtension(NulibState* pState, const char* pathName,
    ulong* pFileType, ulong* pAuxType);
Boolean ExtractPreservationString(NulibState* pState, char* pathname,
    ulong* pFileType, ulong* pAuxType, NuThreadID* pThreadID);
void DenormalizePath(NulibState* pState, char* pathBuf);
const char* FilenameOnly(NulibState* pState, const char* pathname);
const char* FindExtension(NulibState* pState, const char* pathname);

/* List.c */
NuError DoListShort(NulibState* pState);
NuError DoListVerbose(NulibState* pState);
NuError DoListDebug(NulibState* pState);
char* FormatDateShort(const NuDateTime* pDateTime, char* buffer);

/* Main.c */
extern const char* gProgName;

/* MiscUtils.c */
void ReportError(NuError err, const char* format, ...)
    #if defined(__GNUC__)
        __attribute__ ((format(printf, 2, 3)))
    #endif
    ;
#ifdef USE_DMALLOC  /* want file and line numbers for calls */
# define Malloc(size) malloc(size)
# define Calloc(size) calloc(1, size)
# define Realloc(ptr, size) realloc(ptr, size)
# define Free(ptr) (ptr != NULL ? free(ptr) : (void)0)
#else
void* Malloc(size_t size);
void* Calloc(size_t size);
void* Realloc(void* ptr, size_t size);
void Free(void* ptr);
#endif
NuResult FreeCallback(NuArchive* pArchive, void* args);

/* SysUtils.c */
NuError NormalizeFileName(NulibState* pState, const char* srcp, long srcLen,
    char fssep, char** pDstp, long dstLen);
NuError NormalizeDirectoryName(NulibState* pState, const char* srcp,
    long srcLen, char fssep, char** pDstp, long dstLen);
char* MakeTempArchiveName(NulibState* pState);
NuError AddFile(NulibState* pState, NuArchive* pArchive,
    const char* pathname);
NuError Mkdir(const char* dir);
NuError TestFileExistence(const char* fileName, Boolean* pIsDir);

#endif /*__Nulib2__*/
