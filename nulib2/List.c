/*
 * Nulib2
 * Copyright (C) 2000 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 *
 * List files in the archive.
 */
#include "Nulib2.h"


/* kinds of records */
enum RecordKind {
    kRecordKindUnknown = 0,
    kRecordKindDisk,
    kRecordKindFile,
    kRecordKindForkedFile
};

static const char* gShortFormatNames[] = {
    "unc", "squ", "lz1", "lz2", "u12", "u16", "dfl", "bzp"
};


#if 0
/* days of the week */
static const char* gDayNames[] = {
    "[ null ]",
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
#endif
/* months of the year */
static const char* gMonths[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};



/*
 * Compute a percentage.
 */
int
ComputePercent(ulong totalSize, ulong size)
{
    int perc;

    if (!totalSize && !size)
        return 100;     /* file is zero bytes long */

    if (totalSize < 21474836)
        perc = (totalSize * 100) / size;
    else
        perc = totalSize / (size/100);

    /* don't say "0%" if it's not actually zero... it looks dumb */
    if (!perc && size)
        perc = 1;

    return perc;
}


/*
 * Convert a NuDateTime structure into something printable.  This uses an
 * abbreviated format, with date and time but not weekday or seconds.
 *
 * The buffer passed in must hold at least kDateOutputLen bytes.
 *
 * Returns "buffer" for the benefit of printf() calls.
 */
char*
FormatDateShort(const NuDateTime* pDateTime, char* buffer)
{
    /* is it valid? */
    if (pDateTime->day > 30 || pDateTime->month > 11 || pDateTime->hour > 24 ||
        pDateTime->minute > 59)
    {
        strcpy(buffer, "   <invalid>   ");
        goto bail;
    }

    /* is it empty? */
    if ((pDateTime->second | pDateTime->minute | pDateTime->hour |
        pDateTime->year | pDateTime->day | pDateTime->month |
        pDateTime->extra | pDateTime->weekDay) == 0)
    {
        strcpy(buffer, "   [No Date]   ");
        goto bail;
    }

    sprintf(buffer, "%02d-%s-%02d %02d:%02d",
        pDateTime->day+1, gMonths[pDateTime->month], pDateTime->year % 100,
        pDateTime->hour, pDateTime->minute);

bail:
    return buffer;
}


/*
 * NuStream callback function.  Displays the filename.
 */
static NuResult
ShowContentsShort(NuArchive* pArchive, void* vpRecord)
{
    const NuRecord* pRecord = (NuRecord*) vpRecord;
    NulibState* pState;

    Assert(pArchive != nil);
    (void) NuGetExtraData(pArchive, (void**) &pState);
    Assert(pState != nil);

    if (!IsSpecified(pState, pRecord))
        goto bail;

    printf("%s\n",
        pRecord->filename == nil ? "<unknown>":(const char*)pRecord->filename);

bail:
    return kNuOK;
}


/*
 * Analyze the contents of a record to determine if it's a disk, file,
 * or "other".  Compute the total compressed and uncompressed lengths
 * of all data threads.  Return the "best" format.
 *
 * The "best format" and "record type" stuff assume that the entire
 * record contains only a disk thread or a file thread, and that any
 * format is interesting so long as it isn't "no compression".  In
 * general these will be true, because ShrinkIt and NuLib create files
 * this way.
 *
 * You could, of course, create a single record with a data thread and
 * a disk image thread, but it's a fair bet ShrinkIt would ignore one
 * or the other.
 *
 * NOTE: we don't currently work around the GSHK zero-length file bug.
 * Such records, which have a filename thread but no data threads at all,
 * will be categorized as "unknown".  We could detect the situation and
 * correct it, but we might as well flag it in a user-visible way.
 */
static NuError
AnalyzeRecord(const NuRecord* pRecord, enum RecordKind* pRecordKind,
    ushort* pFormat, ulong* pTotalLen, ulong* pTotalCompLen)
{
    const NuThread* pThread;
    NuThreadID threadID;
    ulong idx;

    *pRecordKind = kRecordKindUnknown;
    *pTotalLen = *pTotalCompLen = 0;
    *pFormat = kNuThreadFormatUncompressed;

    for (idx = 0; idx < pRecord->recTotalThreads; idx++) {
        pThread = NuGetThread(pRecord, idx);
        Assert(pThread != nil);

        if (pThread->thThreadClass == kNuThreadClassData) {
            /* replace what's there if this might be more interesting */
            if (*pFormat == kNuThreadFormatUncompressed)
                *pFormat = pThread->thThreadFormat;

            threadID = NuMakeThreadID(pThread->thThreadClass,
                        pThread->thThreadKind);
            if (threadID == kNuThreadIDRsrcFork)
                *pRecordKind = kRecordKindForkedFile;
            else if (threadID == kNuThreadIDDiskImage)
                *pRecordKind = kRecordKindDisk;
            else if (threadID == kNuThreadIDDataFork &&
                                            *pRecordKind == kRecordKindUnknown)
                *pRecordKind = kRecordKindFile;

            /* sum up, so we get both forks of forked files */
            *pTotalLen += pThread->actualThreadEOF;
            *pTotalCompLen += pThread->thCompThreadEOF;
        }
    }

    return kNuErrNone;
}

/*
 * NuStream callback function.  Displays the filename and several attributes.
 *
 * This is intended to mimic the output of some old version of ProDOS 8
 * ShrinkIt.
 */
static NuResult
ShowContentsVerbose(NuArchive* pArchive, void* vpRecord)
{
    NuError err = kNuErrNone;
    const NuRecord* pRecord = (NuRecord*) vpRecord;
    enum RecordKind recordKind;
    ulong totalLen, totalCompLen;
    ushort format;
    NulibState* pState;
    char date1[kDateOutputLen];
    char tmpbuf[16];
    int len;

    Assert(pArchive != nil);
    (void) NuGetExtraData(pArchive, (void**) &pState);
    Assert(pState != nil);

    if (!IsSpecified(pState, pRecord))
        goto bail;

    err = AnalyzeRecord(pRecord, &recordKind, &format, &totalLen,
            &totalCompLen);
    if (err != kNuErrNone)
        goto bail;

    len = strlen(pRecord->filename);
    if (len <= 27) {
        printf("%c%-27.27s ", IsRecordReadOnly(pRecord) ? '+' : ' ',
            pRecord->filename);
    } else {
        printf("%c..%-25.25s ", IsRecordReadOnly(pRecord) ? '+' : ' ',
            pRecord->filename + len - 25);
    }
    switch (recordKind) {
    case kRecordKindUnknown:
        printf("???  $%04lX  ",
            /*GetFileTypeString(pRecord->recFileType),*/
            pRecord->recExtraType);
        break;
    case kRecordKindDisk:
        sprintf(tmpbuf, "%ldk", totalLen / 1024);
        printf("Disk %-6s ", tmpbuf);
        break;
    case kRecordKindFile:
    case kRecordKindForkedFile:
        printf("%s%c $%04lX  ",
            GetFileTypeString(pRecord->recFileType),
            recordKind == kRecordKindForkedFile ? '+' : ' ',
            pRecord->recExtraType);
        break;
    default:
        Assert(0);
        printf("ERROR  ");
    }

    printf("%s  ", FormatDateShort(&pRecord->recArchiveWhen, date1));
    if (format >= NELEM(gShortFormatNames))
        printf("???  ");
    else
        printf("%s  ", gShortFormatNames[format]);

    /* compute the percent size */
    if ((!totalLen && totalCompLen) || (totalLen && !totalCompLen))
        printf("---   ");       /* weird */
    else if (totalLen < totalCompLen)
        printf(">100%% ");      /* compression failed? */
    else {
        sprintf(tmpbuf, "%02d%%", ComputePercent(totalCompLen, totalLen));
        printf("%4s  ", tmpbuf);
    }

    if (!totalLen && totalCompLen)
        printf("   ????");      /* weird */
    else
        printf("%8ld", totalLen);

    printf("\n");

    NState_AddToTotals(pState, totalLen, totalCompLen);

bail:
    if (err != kNuErrNone) {
        printf("(ERROR on '%s')\n", pRecord->filename == nil ?
                "<unknown>" : (const char*)pRecord->filename);
    }
    return kNuOK;
}

/*
 * Print a short listing of the contents of an archive.
 */
NuError
DoListShort(NulibState* pState)
{
    NuError err;
    NuArchive* pArchive = nil;

    Assert(pState != nil);

    if (NState_GetModBinaryII(pState))
        return BNYDoListShort(pState);

    err = OpenArchiveReadOnly(pState);
    if (err == kNuErrIsBinary2)
        return BNYDoListShort(pState);
    if (err != kNuErrNone)
        goto bail;
    pArchive = NState_GetNuArchive(pState);
    Assert(pArchive != nil);

    err = NuContents(pArchive, ShowContentsShort);
    /* fall through with err */

bail:
    if (pArchive != nil)
        (void) NuClose(pArchive);
    return err;
}


/*
 * Print a more verbose listing of the contents of an archive.
 */
NuError
DoListVerbose(NulibState* pState)
{
    NuError err;
    NuArchive* pArchive = nil;
    const NuMasterHeader* pHeader;
    char date1[kDateOutputLen];
    char date2[kDateOutputLen];
    long totalLen, totalCompLen;
    const char* cp;

    Assert(pState != nil);

    if (NState_GetModBinaryII(pState))
        return BNYDoListVerbose(pState);

    err = OpenArchiveReadOnly(pState);
    if (err == kNuErrIsBinary2)
        return BNYDoListVerbose(pState);
    if (err != kNuErrNone)
        goto bail;
    pArchive = NState_GetNuArchive(pState);
    Assert(pArchive != nil);

    /*
     * Try to get just the filename.
     */
    if (IsFilenameStdin(NState_GetArchiveFilename(pState)))
        cp = "<stdin>";
    else
        cp = FilenameOnly(pState, NState_GetArchiveFilename(pState));

    /* grab the master header block */
    err = NuGetMasterHeader(pArchive, &pHeader);
    if (err != kNuErrNone)
        goto bail;

    printf(" %-15.15s Created:%s   Mod:%s     Recs:%5lu\n\n",
        cp,
        FormatDateShort(&pHeader->mhArchiveCreateWhen, date1),
        FormatDateShort(&pHeader->mhArchiveModWhen, date2),
        pHeader->mhTotalRecords);
    printf(" Name                        Type Auxtyp Archived"
           "         Fmat Size Un-Length\n");
    printf("-------------------------------------------------"
           "----------------------------\n");

    err = NuContents(pArchive, ShowContentsVerbose);
    if (err != kNuErrNone)
        goto bail;

    /*
     * Show the totals.  NuFX overhead can be as much as 25% for archives
     * with lots of small files.
     */
    NState_GetTotals(pState, &totalLen, &totalCompLen);
    printf("-------------------------------------------------"
           "----------------------------\n");
    printf(" Uncomp: %ld  Comp: %ld  %%of orig: %d%%\n",
        totalLen, totalCompLen,
        totalLen == 0 ? 0 : ComputePercent(totalCompLen, totalLen));
    #ifdef DEBUG_VERBOSE
    if (!NState_GetFilespecCount(pState)) {
        printf(" Overhead: %ld (%d%%)\n",
            pHeader->mhMasterEOF - totalCompLen,
            ComputePercent(pHeader->mhMasterEOF - totalCompLen,
                pHeader->mhMasterEOF));
    }
    #endif

    /*(void) NuDebugDumpArchive(pArchive);*/

bail:
    if (pArchive != nil)
        (void) NuClose(pArchive);
    return err;
}



/*
 * Null callback, for those times when you don't really want to do anything.
 */
static NuResult
NullCallback(NuArchive* pArchive, void* vpRecord)
{
    return kNuOK;
}

/*
 * Print very detailed output, suitable for debugging (requires that
 * debug messages be enabled in nufxlib).
 */
NuError
DoListDebug(NulibState* pState)
{
    NuError err;
    NuArchive* pArchive = nil;

    Assert(pState != nil);

    if (NState_GetModBinaryII(pState))
        return BNYDoListDebug(pState);

    err = OpenArchiveReadOnly(pState);
    if (err == kNuErrIsBinary2)
        return BNYDoListDebug(pState);
    if (err != kNuErrNone)
        goto bail;
    pArchive = NState_GetNuArchive(pState);
    Assert(pArchive != nil);

    /* have to do something to force the library to scan the archive */
    err = NuContents(pArchive, NullCallback);
    if (err != kNuErrNone)
        goto bail;

    err = NuDebugDumpArchive(pArchive);
    if (err != kNuErrNone)
        fprintf(stderr, "ERROR: debugging not enabled in nufxlib\n");
    /* fall through with err */

bail:
    if (pArchive != nil)
        (void) NuClose(pArchive);
    return err;
}

