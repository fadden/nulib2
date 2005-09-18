/*
 * Nulib2
 * Copyright (C) 2000-2005 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 */
#ifndef __State__
#define __State__

#include "MiscStuff.h"
#include "NufxLib.h"

/*
 * Things you can tell Nulib2 to do.
 *
 * (Some debug code in NState_DebugDump() is sensitive to the order here.)
 */
typedef enum Command {
    kCommandUnknown = 0,
    kCommandAdd,
    kCommandDelete,
    kCommandExtract,
    kCommandExtractToPipe,
    kCommandListShort,
    kCommandListVerbose,
    kCommandListDebug,
    kCommandTest,
    kCommandHelp
} Command;


/*
 * Program-wide state.
 */
typedef struct NulibState {
    /* global goodness */
    const char*     programVersion;

    /* system-specific values */
    char            systemPathSeparator;
    char            altSystemPathSeparator;

    /* pointer to archive we're working with */
    NuArchive*      pArchive;

    /* misc state */
    Boolean         suppressOutput;
    Boolean         inputUnavailable;
    NuRecordIdx     renameFromIdx;
    char*           renameToStr;
    NuDataSink*     pPipeSink;
    NuDataSink*     pCommentSink;
    long            matchCount;
    long            totalLen;
    long            totalCompLen;

    /* temp storage */
    long            tempPathnameAlloc;
    char*           tempPathnameBuf;

    /* command-line options */
    Command         command;
    Boolean         modUpdate;
    Boolean         modFreshen;
    Boolean         modRecurse;
    Boolean         modJunkPaths;
    Boolean         modNoCompression;
    Boolean         modCompressDeflate;
    Boolean         modCompressBzip2;
    Boolean         modComments;
    Boolean         modBinaryII;
    Boolean         modConvertText;
    Boolean         modConvertAll;
    Boolean         modOverwriteExisting;
    Boolean         modAddAsDisk;
    Boolean         modPreserveType;
    Boolean         modPreserveTypeExtended;

    const char*     archiveFilename;
    char* const*    filespecPointer;
    long            filespecCount;
} NulibState;

NuError NState_Init(NulibState** ppState);
NuError NState_ExtraInit(NulibState* pState);
void NState_Free(NulibState* pState);
#ifdef DEBUG_MSGS
void NState_DebugDump(const NulibState* pState);
#endif

char NState_GetSystemPathSeparator(const NulibState* pState);
char NState_GetAltSystemPathSeparator(const NulibState* pState);
const char* NState_GetProgramVersion(const NulibState* pState);
NuArchive* NState_GetNuArchive(const NulibState* pState);
void NState_SetNuArchive(NulibState* pState, NuArchive* pArchive);

Boolean NState_GetSuppressOutput(const NulibState* pState);
void NState_SetSuppressOutput(NulibState* pState, Boolean doSuppress);
Boolean NState_GetInputUnavailable(const NulibState* pState);
void NState_SetInputUnavailable(NulibState* pState, Boolean isUnavailable);
NuRecordIdx NState_GetRenameFromIdx(const NulibState* pState);
void NState_SetRenameFromIdx(NulibState* pState, NuRecordIdx recordIdx);
char* NState_GetRenameToStr(const NulibState* pState);
void NState_SetRenameToStr(NulibState* pState, char* str);
NuDataSink* NState_GetPipeSink(const NulibState* pState);
NuDataSink* NState_GetCommentSink(const NulibState* pState);
long NState_GetMatchCount(const NulibState* pState);
void NState_SetMatchCount(NulibState* pState, long count);
void NState_IncMatchCount(NulibState* pState);
void NState_AddToTotals(NulibState* pState, long len, long compLen);
void NState_GetTotals(NulibState* pState, long* pTotalLen, long* pTotalCompLen);

long NState_GetTempPathnameLen(NulibState* pState);
void NState_SetTempPathnameLen(NulibState* pState, long len);
char* NState_GetTempPathnameBuf(NulibState* pState);

Command NState_GetCommand(const NulibState* pState);
void NState_SetCommand(NulibState* pState, Command cmd);
const char* NState_GetArchiveFilename(const NulibState* pState);
void NState_SetArchiveFilename(NulibState* pState, const char* archiveFilename);
char* const* NState_GetFilespecPointer(const NulibState* pState);
void NState_SetFilespecPointer(NulibState* pState, char* const* filespec);
long NState_GetFilespecCount(const NulibState* pState);
void NState_SetFilespecCount(NulibState* pState, long filespecCount);

Boolean NState_GetModUpdate(const NulibState* pState);
void NState_SetModUpdate(NulibState* pState, Boolean val);
Boolean NState_GetModFreshen(const NulibState* pState);
void NState_SetModFreshen(NulibState* pState, Boolean val);
Boolean NState_GetModRecurse(const NulibState* pState);
void NState_SetModRecurse(NulibState* pState, Boolean val);
Boolean NState_GetModJunkPaths(const NulibState* pState);
void NState_SetModJunkPaths(NulibState* pState, Boolean val);
Boolean NState_GetModNoCompression(const NulibState* pState);
void NState_SetModNoCompression(NulibState* pState, Boolean val);
Boolean NState_GetModCompressDeflate(const NulibState* pState);
void NState_SetModCompressDeflate(NulibState* pState, Boolean val);
Boolean NState_GetModCompressBzip2(const NulibState* pState);
void NState_SetModCompressBzip2(NulibState* pState, Boolean val);
Boolean NState_GetModComments(const NulibState* pState);
void NState_SetModComments(NulibState* pState, Boolean val);
Boolean NState_GetModBinaryII(const NulibState* pState);
void NState_SetModBinaryII(NulibState* pState, Boolean val);
Boolean NState_GetModConvertText(const NulibState* pState);
void NState_SetModConvertText(NulibState* pState, Boolean val);
Boolean NState_GetModConvertAll(const NulibState* pState);
void NState_SetModConvertAll(NulibState* pState, Boolean val);
Boolean NState_GetModOverwriteExisting(const NulibState* pState);
void NState_SetModOverwriteExisting(NulibState* pState, Boolean val);
Boolean NState_GetModAddAsDisk(const NulibState* pState);
void NState_SetModAddAsDisk(NulibState* pState, Boolean val);
Boolean NState_GetModPreserveType(const NulibState* pState);
void NState_SetModPreserveType(NulibState* pState, Boolean val);
Boolean NState_GetModPreserveTypeExtended(const NulibState* pState);
void NState_SetModPreserveTypeExtended(NulibState* pState, Boolean val);

#endif /*__State__*/
