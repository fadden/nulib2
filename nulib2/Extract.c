/*
 * Nulib2
 * Copyright (C) 2000-2004 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 *
 * Extract files and test archives.
 */
#include "Nulib2.h"


/*
 * Extract all of the records from the archive, pulling out and displaying
 * comment threads.
 *
 * The "bulk extract" call doesn't deal with comments.  Since we want to
 * show them while we're extracting the files, we have to manually find
 * and extract them.
 */
static NuError
ExtractAllRecords(NulibState* pState, NuArchive* pArchive)
{
    NuError err;
    const NuRecord* pRecord;
    const NuThread* pThread;
    NuRecordIdx recordIdx;
    NuAttr numRecords;
    int idx, threadIdx;

    DBUG(("--- doing manual extract\n"));
    Assert(NState_GetCommand(pState) == kCommandExtract);   /* no "-p" here */

    err = NuGetAttr(pArchive, kNuAttrNumRecords, &numRecords);
    for (idx = 0; idx < (int) numRecords; idx++) {
        err = NuGetRecordIdxByPosition(pArchive, idx, &recordIdx);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: couldn't get record #%d (err=%d)\n",
                idx, err);
            goto bail;
        }

        err = NuGetRecord(pArchive, recordIdx, &pRecord);
        if (err != kNuErrNone) {
            fprintf(stderr, "ERROR: unable to get recordIdx %ld\n", recordIdx);
            goto bail;
        }

        /* do we want to extract this record? */
        if (!IsSpecified(pState, pRecord))
            continue;
        NState_IncMatchCount(pState);

        /*
         * Look for a comment thread.
         */
        for (threadIdx = 0; (ulong)threadIdx < pRecord->recTotalThreads;
            threadIdx++)
        {
            pThread = NuGetThread(pRecord, threadIdx);
            Assert(pThread != nil);

            if (NuGetThreadID(pThread) == kNuThreadIDComment &&
                pThread->actualThreadEOF > 0)
            {
                printf("----- '%s':\n", pRecord->filename);
                err = NuExtractThread(pArchive, pThread->threadIdx,
                        NState_GetCommentSink(pState));
                if (err != kNuErrNone) {
                    printf("[comment extraction failed, continuing\n");
                } else {
                    printf("\n-----\n");
                }
            }
        }

        /* extract the record, using the usual mechanisms */
        err = NuExtractRecord(pArchive, recordIdx);
        if (err != kNuErrNone)
            goto bail;
    }

bail:
    return err;
}


/*
 * Extract the specified files.
 */
NuError
DoExtract(NulibState* pState)
{
    NuError err;
    NuArchive* pArchive = nil;

    Assert(pState != nil);

    if (NState_GetModBinaryII(pState))
        return BNYDoExtract(pState);

    err = OpenArchiveReadOnly(pState);
    if (err == kNuErrIsBinary2)
        return BNYDoExtract(pState);
    if (err != kNuErrNone)
        goto bail;
    pArchive = NState_GetNuArchive(pState);
    Assert(pArchive != nil);

    NState_SetMatchCount(pState, 0);

    /*
     * If we're not interested in comments, just use the "bulk" extract
     * call.  If we want comments, we need to do this one at a time.
     */
    if (!NState_GetModComments(pState)) {
        err = NuExtract(pArchive);
        if (err != kNuErrNone)
            goto bail;
    } else {
        err = ExtractAllRecords(pState, pArchive);
        if (err != kNuErrNone)
            goto bail;
    }

    if (!NState_GetMatchCount(pState))
        printf("%s: no records match\n", gProgName);

bail:
    if (pArchive != nil)
        (void) NuClose(pArchive);
    return err;
}


/*
 * Extract the specified files to stdout.
 */
NuError
DoExtractToPipe(NulibState* pState)
{
    /* we handle the "to pipe" part farther down */
    return DoExtract(pState);
}


/*
 * Do an integrity check on one or more records in the archive.
 */
NuError
DoTest(NulibState* pState)
{
    NuError err;
    NuArchive* pArchive = nil;

    Assert(pState != nil);

    if (NState_GetModBinaryII(pState))
        return BNYDoTest(pState);

    err = OpenArchiveReadOnly(pState);
    if (err == kNuErrIsBinary2)
        return BNYDoTest(pState);
    if (err != kNuErrNone)
        goto bail;
    pArchive = NState_GetNuArchive(pState);
    Assert(pArchive != nil);

    NState_SetMatchCount(pState, 0);

    err = NuTest(pArchive);
    if (err != kNuErrNone)
        goto bail;

    if (!NState_GetMatchCount(pState))
        printf("%s: no records match\n", gProgName);

bail:
    if (pArchive != nil)
        (void) NuClose(pArchive);
    return err;
}

