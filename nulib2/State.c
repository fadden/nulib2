/*
 * Nulib2
 * Copyright (C) 2000-2002 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 *
 * Global-ish state object.
 */
#include "Nulib2.h"


static const char* gProgramVersion = "1.1.0";


/*
 * Allocate and initialize the semi-global Nulib2 state object.
 */
NuError
NState_Init(NulibState** ppState)
{
    Assert(ppState != nil);

    *ppState = Calloc(sizeof(**ppState));
    if (*ppState == nil)
        return kNuErrMalloc;

    /*
     * Initialize the contents to default values.
     */
    (*ppState)->systemPathSeparator = PATH_SEP;
    (*ppState)->programVersion = gProgramVersion;

    return kNuErrNone;
}

/*
 * A little extra initialization, performed after arguments are parsed.
 */
NuError
NState_ExtraInit(NulibState* pState)
{
    NuError err;
    NuValue convertEOL;

    /*
     * Create a data sink for "stdout", in case we use the "-p" command.
     * Set the EOL conversion according to the "-l" modifier.
     */
    convertEOL = kNuConvertOff;
    if (pState->modConvertText)
        convertEOL = kNuConvertAuto;
    if (pState->modConvertAll)
        convertEOL = kNuConvertOn;

    err = NuCreateDataSinkForFP(true, convertEOL, stdout, &pState->pPipeSink);
    if (err != kNuErrNone)
        return err;

    /*
     * Create a data sink for "stdout", in case we use the "-c" modifier.
     * The EOL conversion is always on.
     */
    err = NuCreateDataSinkForFP(true, kNuConvertOn, stdout,
            &pState->pCommentSink);
    return err;
}


/*
 * Free up the state structure and its contents.
 */
void
NState_Free(NulibState* pState)
{
    if (pState == nil)
        return;

    Free(pState->renameToStr);  /* ?? */
    Free(pState->tempPathnameBuf);
    if (pState->pPipeSink != nil)
        NuFreeDataSink(pState->pPipeSink);
    if (pState->pCommentSink != nil)
        NuFreeDataSink(pState->pCommentSink);
    Free(pState);
}


#ifdef DEBUG_MSGS
void
NState_DebugDump(const NulibState* pState)
{
    /* this table will break if the code changes, but it's just for debugging */
    static const char* kCommandNames[] = {
        "<unknown>",
        "add",
        "delete",
        "extract",
        "extractToPipe",
        "listShort",
        "listVerbose",
        "listDebug",
        "test",
        "help",
    };

    Assert(pState != nil);

    printf("NState:\n");
    printf("  programVersion: '%s'\n", pState->programVersion);
    printf("  systemPathSeparator: '%c'\n", pState->systemPathSeparator);
    printf("  archiveFilename: '%s'\n", pState->archiveFilename);
    printf("  filespec: %ld (%s ...)\n", pState->filespecCount,
        !pState->filespecCount ? "<none>" : *pState->filespecPointer);

    printf("  command: %d (%s); modifiers:\n", pState->command,
        kCommandNames[pState->command]);
    if (pState->modUpdate)
        printf("    update\n");
    if (pState->modFreshen)
        printf("    freshen\n");
    if (pState->modRecurse)
        printf("    recurse\n");
    if (pState->modJunkPaths)
        printf("    junkPaths\n");
    if (pState->modNoCompression)
        printf("    noCompression\n");
    if (pState->modCompressDeflate)
        printf("    compressDeflate\n");
    if (pState->modCompressBzip2)
        printf("    compressBzip2\n");
    if (pState->modComments)
        printf("    comments\n");
    if (pState->modBinaryII)
        printf("    binaryII\n");
    if (pState->modConvertText)
        printf("    convertText\n");
    if (pState->modConvertAll)
        printf("    convertAll\n");
    if (pState->modOverwriteExisting)
        printf("    overwriteExisting\n");
    if (pState->modPreserveType)
        printf("    preserveType\n");
    if (pState->modPreserveTypeExtended)
        printf("    preserveTypeExtended\n");

    printf("\n");
}
#endif


/*
 * ===========================================================================
 *      Simple set/get functions
 * ===========================================================================
 */

char
NState_GetSystemPathSeparator(const NulibState* pState)
{
    return pState->systemPathSeparator;
}

const char*
NState_GetProgramVersion(const NulibState* pState)
{
    return pState->programVersion;
}

NuArchive*
NState_GetNuArchive(const NulibState* pState)
{
    return pState->pArchive;
}

void
NState_SetNuArchive(NulibState* pState, NuArchive* pArchive)
{
    pState->pArchive = pArchive;
}


Boolean
NState_GetSuppressOutput(const NulibState* pState)
{
    return pState->suppressOutput;
}

void
NState_SetSuppressOutput(NulibState* pState, Boolean doSuppress)
{
    pState->suppressOutput = doSuppress;
}

Boolean
NState_GetInputUnavailable(const NulibState* pState)
{
    return pState->inputUnavailable;
}

void
NState_SetInputUnavailable(NulibState* pState, Boolean isUnavailable)
{
    pState->inputUnavailable = isUnavailable;
}

NuRecordIdx
NState_GetRenameFromIdx(const NulibState* pState)
{
    return pState->renameFromIdx;
}

void
NState_SetRenameFromIdx(NulibState* pState, NuRecordIdx recordIdx)
{
    pState->renameFromIdx = recordIdx;
}

char*
NState_GetRenameToStr(const NulibState* pState)
{
    return pState->renameToStr;
}

void
NState_SetRenameToStr(NulibState* pState, char* str)
{
    Free(pState->renameToStr);
    pState->renameToStr = str;
}


NuDataSink*
NState_GetPipeSink(const NulibState* pState)
{
    return pState->pPipeSink;
}

NuDataSink*
NState_GetCommentSink(const NulibState* pState)
{
    return pState->pCommentSink;
}

long
NState_GetMatchCount(const NulibState* pState)
{
    return pState->matchCount;
}

void
NState_SetMatchCount(NulibState* pState, long count)
{
    pState->matchCount = count;
}

void
NState_IncMatchCount(NulibState* pState)
{
    pState->matchCount++;
}

void
NState_AddToTotals(NulibState* pState, long len, long compLen)
{
    pState->totalLen += len;
    pState->totalCompLen += compLen;
}

void
NState_GetTotals(NulibState* pState, long* pTotalLen, long* pTotalCompLen)
{
    *pTotalLen = pState->totalLen;
    *pTotalCompLen = pState->totalCompLen;
}

long
NState_GetTempPathnameLen(NulibState* pState)
{
    return pState->tempPathnameAlloc;
}

void
NState_SetTempPathnameLen(NulibState* pState, long len)
{
    char* newBuf;

    len++;      /* add one for the '\0' */

    if (pState->tempPathnameAlloc < len) {
        if (pState->tempPathnameBuf == nil)
            newBuf = Malloc(len);
        else
            newBuf = Realloc(pState->tempPathnameBuf, len);
        Assert(newBuf != nil);
        if (newBuf == nil) {
            Free(pState->tempPathnameBuf);
            pState->tempPathnameBuf = nil;
            pState->tempPathnameAlloc = 0;
            ReportError(kNuErrMalloc, "buf realloc failed (%ld)", len);
            return;
        }

        pState->tempPathnameBuf = newBuf;
        pState->tempPathnameAlloc = len;
    }
}

char*
NState_GetTempPathnameBuf(NulibState* pState)
{
    return pState->tempPathnameBuf;
}


Command
NState_GetCommand(const NulibState* pState)
{
    return pState->command;
}

void
NState_SetCommand(NulibState* pState, Command cmd)
{
    pState->command = cmd;
}

const char*
NState_GetArchiveFilename(const NulibState* pState)
{
    return pState->archiveFilename;
}

void
NState_SetArchiveFilename(NulibState* pState, const char* archiveFilename)
{
    pState->archiveFilename = archiveFilename;
}

char* const*
NState_GetFilespecPointer(const NulibState* pState)
{
    return pState->filespecPointer;
}

void
NState_SetFilespecPointer(NulibState* pState, char* const* filespecPointer)
{
    pState->filespecPointer = filespecPointer;
}

long
NState_GetFilespecCount(const NulibState* pState)
{
    return pState->filespecCount;
}

void
NState_SetFilespecCount(NulibState* pState, long filespecCount)
{
    pState->filespecCount = filespecCount;
}

Boolean
NState_GetModUpdate(const NulibState* pState)
{
    return pState->modUpdate;
}

void
NState_SetModUpdate(NulibState* pState, Boolean val)
{
    pState->modUpdate = val;
}

Boolean
NState_GetModFreshen(const NulibState* pState)
{
    return pState->modFreshen;
}

void
NState_SetModFreshen(NulibState* pState, Boolean val)
{
    pState->modFreshen = val;
}

Boolean
NState_GetModRecurse(const NulibState* pState)
{
    return pState->modRecurse;
}

void
NState_SetModRecurse(NulibState* pState, Boolean val)
{
    pState->modRecurse = val;
}

Boolean
NState_GetModJunkPaths(const NulibState* pState)
{
    return pState->modJunkPaths;
}

void
NState_SetModJunkPaths(NulibState* pState, Boolean val)
{
    pState->modJunkPaths = val;
}

Boolean
NState_GetModNoCompression(const NulibState* pState)
{
    return pState->modNoCompression;
}

void
NState_SetModNoCompression(NulibState* pState, Boolean val)
{
    pState->modNoCompression = val;
}

Boolean
NState_GetModCompressDeflate(const NulibState* pState)
{
    return pState->modCompressDeflate;
}

void
NState_SetModCompressDeflate(NulibState* pState, Boolean val)
{
    pState->modCompressDeflate = val;
}

Boolean
NState_GetModCompressBzip2(const NulibState* pState)
{
    return pState->modCompressBzip2;
}

void
NState_SetModCompressBzip2(NulibState* pState, Boolean val)
{
    pState->modCompressBzip2 = val;
}

Boolean
NState_GetModComments(const NulibState* pState)
{
    return pState->modComments;
}

void
NState_SetModComments(NulibState* pState, Boolean val)
{
    pState->modComments = val;
}

Boolean
NState_GetModBinaryII(const NulibState* pState)
{
    return pState->modBinaryII;
}

void
NState_SetModBinaryII(NulibState* pState, Boolean val)
{
    pState->modBinaryII = val;
}

Boolean
NState_GetModConvertText(const NulibState* pState)
{
    return pState->modConvertText;
}

void
NState_SetModConvertText(NulibState* pState, Boolean val)
{
    pState->modConvertText = val;
}

Boolean
NState_GetModConvertAll(const NulibState* pState)
{
    return pState->modConvertAll;
}

void
NState_SetModConvertAll(NulibState* pState, Boolean val)
{
    pState->modConvertAll = val;
}

Boolean
NState_GetModOverwriteExisting(const NulibState* pState)
{
    return pState->modOverwriteExisting;
}

void
NState_SetModOverwriteExisting(NulibState* pState, Boolean val)
{
    pState->modOverwriteExisting = val;
}

Boolean
NState_GetModAddAsDisk(const NulibState* pState)
{
    return pState->modAddAsDisk;
}

void
NState_SetModAddAsDisk(NulibState* pState, Boolean val)
{
    pState->modAddAsDisk = val;
}

Boolean
NState_GetModPreserveType(const NulibState* pState)
{
    return pState->modPreserveType;
}

void
NState_SetModPreserveType(NulibState* pState, Boolean val)
{
    pState->modPreserveType = val;
}

Boolean
NState_GetModPreserveTypeExtended(const NulibState* pState)
{
    return pState->modPreserveTypeExtended;
}

void
NState_SetModPreserveTypeExtended(NulibState* pState, Boolean val)
{
    pState->modPreserveTypeExtended = val;
}

