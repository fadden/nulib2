/*
 * NuLib2
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.
 *
 * List files in the archive.
 */
#include "NuLib2.h"


/* kinds of records */
enum RecordKind {
    kRecordKindUnknown = 0,
    kRecordKindDisk,
    kRecordKindFile,
    kRecordKindForkedFile
};

static const char* gShortFormatNames[] = {
    "unc", "squ", "lz1", "lz2", "u12", "u16", "dfl", "bzp", "zx0"
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
int ComputePercent(uint32_t totalSize, uint32_t size)
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
char* FormatDateShort(const NuDateTime* pDateTime, char* buffer)
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
static NuResult ShowContentsShort(NuArchive* pArchive, void* vpRecord)
{
    const NuRecord* pRecord = (NuRecord*) vpRecord;
    NulibState* pState;

    Assert(pArchive != NULL);
    (void) NuGetExtraData(pArchive, (void**) &pState);
    Assert(pState != NULL);

    if (!IsSpecified(pState, pRecord))
        goto bail;

    UNICHAR* filenameUNI = CopyMORToUNI(pRecord->filenameMOR);
    printf("%s\n", filenameUNI == NULL ?  "<unknown>" : filenameUNI);
    free(filenameUNI);

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
 * NOTE: GSHK likes to omit the data threads for zero-length data and
 * resource forks.  That screws up analysis by scanning for threads.  We
 * can work around missing resource forks by simply checking the record's
 * storage type.  We could be clever and detect records that have no
 * data-class threads at all, and no additional threads other than a
 * comment and filename, but this is just for display.  ShrinkIt doesn't
 * handle these records correctly in all cases, so flagging them in a
 * user-visible way seems reasonable.
 */
static NuError AnalyzeRecord(const NuRecord* pRecord,
    enum RecordKind* pRecordKind, uint16_t* pFormat, uint32_t* pTotalLen,
    uint32_t* pTotalCompLen)
{
    const NuThread* pThread;
    NuThreadID threadID;
    uint32_t idx;

    *pRecordKind = kRecordKindUnknown;
    *pTotalLen = *pTotalCompLen = 0;
    *pFormat = kNuThreadFormatUncompressed;

    for (idx = 0; idx < pRecord->recTotalThreads; idx++) {
        pThread = NuGetThread(pRecord, idx);
        Assert(pThread != NULL);

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

    /*
     * Fix up the case where we have a forked file, but GSHK decided not
     * to include a resource fork in the record.
     */
    if (pRecord->recStorageType == kNuStorageExtended &&
        *pRecordKind != kRecordKindForkedFile)
    {
        *pRecordKind = kRecordKindForkedFile;
    }

    return kNuErrNone;
}

/*
 * NuStream callback function.  Displays the filename and several attributes.
 *
 * This is intended to mimic the output of some old version of ProDOS 8
 * ShrinkIt.
 */
static NuResult ShowContentsVerbose(NuArchive* pArchive, void* vpRecord)
{
    NuError err = kNuErrNone;
    const NuRecord* pRecord = (NuRecord*) vpRecord;
    enum RecordKind recordKind;
    uint32_t totalLen, totalCompLen;
    uint16_t format;
    NulibState* pState;
    char date1[kDateOutputLen];
    char tmpbuf[16];
    int len;

    Assert(pArchive != NULL);
    (void) NuGetExtraData(pArchive, (void**) &pState);
    Assert(pState != NULL);

    if (!IsSpecified(pState, pRecord))
        goto bail;

    err = AnalyzeRecord(pRecord, &recordKind, &format, &totalLen,
            &totalCompLen);
    if (err != kNuErrNone)
        goto bail;

    /*
     * Display the filename, truncating if it's longer than 27 characters.
     *
     * Attempting to do column layout with printf string formatting (e.g.
     * "%-27s") doesn't really work for UTF-8 because printf() is
     * operating on bytes, and the conversion to a Unicode code point
     * is happening in the terminal.  We need to do the spacing ourselves,
     * using the fact that one MOR character turns into one Unicode character.
     *
     * If the display isn't converting multi-byte sequences to individual
     * characters, this won't look right, but we can't make everybody happy.
     */
    static const char kSpaces[27+1] = "                           ";
    UNICHAR* filenameUNI;
    len = strlen(pRecord->filenameMOR);
    if (len <= 27) {
        filenameUNI = CopyMORToUNI(pRecord->filenameMOR);
        printf("%c%s%s ", IsRecordReadOnly(pRecord) ? '+' : ' ',
            filenameUNI, &kSpaces[len]);
    } else {
        filenameUNI = CopyMORToUNI(pRecord->filenameMOR + len - 25);
        printf("%c..%s ", IsRecordReadOnly(pRecord) ? '+' : ' ',
            filenameUNI);
    }
    free(filenameUNI);

    switch (recordKind) {
    case kRecordKindUnknown:
        printf("%s- $%04X  ",
            GetFileTypeString(pRecord->recFileType),
            pRecord->recExtraType);
        break;
    case kRecordKindDisk:
        sprintf(tmpbuf, "%dk", totalLen / 1024);
        printf("Disk %-6s ", tmpbuf);
        break;
    case kRecordKindFile:
    case kRecordKindForkedFile:
        printf("%s%c $%04X  ",
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
        printf("%8u", totalLen);

    printf("\n");

    NState_AddToTotals(pState, totalLen, totalCompLen);

bail:
    if (err != kNuErrNone) {
        filenameUNI = CopyMORToUNI(pRecord->filenameMOR);
        printf("(ERROR on '%s')\n",
                filenameUNI == NULL ?  "<unknown>" : filenameUNI);
        free(filenameUNI);
    }
    return kNuOK;
}

/*
 * Print a short listing of the contents of an archive.
 */
NuError DoListShort(NulibState* pState)
{
    NuError err;
    NuArchive* pArchive = NULL;

    Assert(pState != NULL);

    if (NState_GetModBinaryII(pState))
        return BNYDoListShort(pState);

    err = OpenArchiveReadOnly(pState);
    if (err == kNuErrIsBinary2)
        return BNYDoListShort(pState);
    if (err != kNuErrNone)
        goto bail;
    pArchive = NState_GetNuArchive(pState);
    Assert(pArchive != NULL);

    err = NuContents(pArchive, ShowContentsShort);
    /* fall through with err */

bail:
    if (pArchive != NULL)
        (void) NuClose(pArchive);
    return err;
}


/*
 * Print a more verbose listing of the contents of an archive.
 */
NuError DoListVerbose(NulibState* pState)
{
    NuError err;
    NuArchive* pArchive = NULL;
    const NuMasterHeader* pHeader;
    char date1[kDateOutputLen];
    char date2[kDateOutputLen];
    long totalLen, totalCompLen;
    const char* cp;

    Assert(pState != NULL);

    if (NState_GetModBinaryII(pState))
        return BNYDoListVerbose(pState);

    err = OpenArchiveReadOnly(pState);
    if (err == kNuErrIsBinary2)
        return BNYDoListVerbose(pState);
    if (err != kNuErrNone)
        goto bail;
    pArchive = NState_GetNuArchive(pState);
    Assert(pArchive != NULL);

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

    printf(" %-15.15s Created:%s   Mod:%s     Recs:%5u\n\n",
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
    if (pArchive != NULL)
        (void) NuClose(pArchive);
    return err;
}



/*
 * Null callback, for those times when you don't really want to do anything.
 */
static NuResult NullCallback(NuArchive* pArchive, void* vpRecord)
{
    return kNuOK;
}

/*
 * Print very detailed output, suitable for debugging (requires that
 * debug messages be enabled in nufxlib).
 */
NuError DoListDebug(NulibState* pState)
{
    NuError err;
    NuArchive* pArchive = NULL;

    Assert(pState != NULL);

    if (NState_GetModBinaryII(pState))
        return BNYDoListDebug(pState);

    err = OpenArchiveReadOnly(pState);
    if (err == kNuErrIsBinary2)
        return BNYDoListDebug(pState);
    if (err != kNuErrNone)
        goto bail;
    pArchive = NState_GetNuArchive(pState);
    Assert(pArchive != NULL);

    /* have to do something to force the library to scan the archive */
    err = NuContents(pArchive, NullCallback);
    if (err != kNuErrNone)
        goto bail;

    err = NuDebugDumpArchive(pArchive);
    if (err != kNuErrNone)
        fprintf(stderr, "ERROR: debugging not enabled in nufxlib\n");
    /* fall through with err */

bail:
    if (pArchive != NULL)
        (void) NuClose(pArchive);
    return err;
}

