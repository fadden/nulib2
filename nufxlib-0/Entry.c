/*
 * NuFX archive manipulation library
 * Copyright (C) 2000 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Library General Public License, see the file COPYING.LIB.
 *
 * All external entry points.
 */
#include "NufxLibPriv.h"


/*
 * ===========================================================================
 *      Misc utils
 * ===========================================================================
 */

/*
 * Set the busy flag.
 *
 * The busy flag is intended to prevent the caller from executing illegal
 * operations while inside a callback function.  It is NOT intended to
 * allow concurrent access to the same archive from multiple threads, so
 * it does not follow all sorts of crazy semaphore semantics.  If you
 * have the need, go ahead and fix it.
 */
static inline void
Nu_SetBusy(NuArchive* pArchive)
{
    pArchive->busy = true;
}

/*
 * Clear the busy flag.
 */
static inline void
Nu_ClearBusy(NuArchive* pArchive)
{
    pArchive->busy = false;
}


/*
 * Do a partial validation on NuArchive.  Some calls, such as GetExtraData,
 * can be made during callback functions when the archive isn't fully
 * consistent.
 */
static NuError
Nu_PartiallyValidateNuArchive(const NuArchive* pArchive)
{
    if (pArchive == nil)
        return kNuErrInvalidArg;

    pArchive =  pArchive;
    if (pArchive->structMagic != kNuArchiveStructMagic)
        return kNuErrBadStruct;

    return kNuErrNone;
}

/*
 * Validate the NuArchive* argument passed in to us.
 */
static NuError
Nu_ValidateNuArchive(const NuArchive* pArchive)
{
    NuError err;

    err = Nu_PartiallyValidateNuArchive(pArchive);
    if (err != kNuErrNone)
        return err;

    /* explicitly block reentrant calls */
    if (pArchive->busy)
        return kNuErrBusy;

    /* make sure the TOC state is consistent */
    if (pArchive->haveToc) {
        if (pArchive->masterHeader.mhTotalRecords != 0)
            Assert(Nu_RecordSet_GetListHead(&pArchive->origRecordSet) != nil);
        Assert(Nu_RecordSet_GetNumRecords(&pArchive->origRecordSet) ==
               pArchive->masterHeader.mhTotalRecords);
    } else {
        Assert(Nu_RecordSet_GetListHead(&pArchive->origRecordSet) == nil);
    }

    /* make sure we have open files to work with */
    Assert(pArchive->archivePathname == nil || pArchive->archiveFp != nil);
    if (pArchive->archivePathname != nil && pArchive->archiveFp == nil)
        return kNuErrInternal;
    Assert(pArchive->tmpPathname == nil || pArchive->tmpFp != nil);
    if (pArchive->tmpPathname != nil && pArchive->tmpFp == nil)
        return kNuErrInternal;

    /* further validations */

    return kNuErrNone;
}


/*
 * ===========================================================================
 *      Streaming and non-streaming read-only
 * ===========================================================================
 */

NuError
NuStreamOpenRO(FILE* infp, NuArchive** ppArchive)
{
    NuError err;

    if (infp == nil || ppArchive == nil)
        return kNuErrInvalidArg;

    err = Nu_StreamOpenRO(infp, (NuArchive**) ppArchive);

    return err;
}

NuError
NuContents(NuArchive* pArchive, NuCallback contentFunc)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        if (Nu_IsStreaming(pArchive))
            err = Nu_StreamContents(pArchive, contentFunc);
        else
            err = Nu_Contents(pArchive, contentFunc);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuExtract(NuArchive* pArchive)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        if (Nu_IsStreaming(pArchive))
            err = Nu_StreamExtract(pArchive);
        else
            err = Nu_Extract(pArchive);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuTest(NuArchive* pArchive)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        if (Nu_IsStreaming(pArchive))
            err = Nu_StreamTest(pArchive);
        else
            err = Nu_Test(pArchive);
        Nu_ClearBusy(pArchive);
    }

    return err;
}


/*
 * ===========================================================================
 *      Strictly non-streaming read-only
 * ===========================================================================
 */

NuError
NuOpenRO(const char* filename, NuArchive** ppArchive)
{
    NuError err;

    err = Nu_OpenRO(filename, (NuArchive**) ppArchive);

    return err;
}

NuError
NuExtractRecord(NuArchive* pArchive, NuRecordIdx recordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_ExtractRecord(pArchive, recordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuExtractThread(NuArchive* pArchive, NuThreadIdx threadIdx,
    NuDataSink* pDataSink)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_ExtractThread(pArchive, threadIdx, pDataSink);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuGetRecord(NuArchive* pArchive, NuRecordIdx recordIdx,
    const NuRecord** ppRecord)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_GetRecord(pArchive, recordIdx, ppRecord);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuGetRecordIdxByName(NuArchive* pArchive, const char* name,
    NuRecordIdx* pRecordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_GetRecordIdxByName(pArchive, name, pRecordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuGetRecordIdxByPosition(NuArchive* pArchive, unsigned long position,
    NuRecordIdx* pRecordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_GetRecordIdxByPosition(pArchive, position, pRecordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}


/*
 * ===========================================================================
 *      Read/Write
 * ===========================================================================
 */

NuError
NuOpenRW(const char* archivePathname, const char* tmpPathname,
    unsigned long flags, NuArchive** ppArchive)
{
    NuError err;

    err = Nu_OpenRW(archivePathname, tmpPathname, flags,
            (NuArchive**) ppArchive);

    return err;
}

NuError
NuFlush(NuArchive* pArchive, long* pStatusFlags)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_Flush(pArchive, pStatusFlags);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuAbort(NuArchive* pArchive)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_Abort(pArchive);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuAddRecord(NuArchive* pArchive, const NuFileDetails* pFileDetails,
    NuRecordIdx* pRecordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_AddRecord(pArchive, pFileDetails, pRecordIdx, nil);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuAddThread(NuArchive* pArchive, NuRecordIdx recordIdx, NuThreadID threadID,
    NuDataSource* pDataSource, NuThreadIdx* pThreadIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_AddThread(pArchive, recordIdx, threadID,
                pDataSource, pThreadIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuAddFile(NuArchive* pArchive, const char* pathname,
    const NuFileDetails* pFileDetails, short isFromRsrcFork,
    NuRecordIdx* pRecordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_AddFile(pArchive, pathname, pFileDetails,
                (Boolean)(isFromRsrcFork != 0), pRecordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuRename(NuArchive* pArchive, NuRecordIdx recordIdx, const char* pathname,
    char fssep)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_Rename(pArchive, recordIdx, pathname, fssep);
        Nu_ClearBusy(pArchive);
    }

    return err;
}


NuError
NuSetRecordAttr(NuArchive* pArchive, NuRecordIdx recordIdx,
    const NuRecordAttr* pRecordAttr)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_SetRecordAttr(pArchive, recordIdx, pRecordAttr);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuUpdatePresizedThread(NuArchive* pArchive, NuThreadIdx threadIdx,
    NuDataSource* pDataSource, long* pMaxLen)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_UpdatePresizedThread(pArchive, threadIdx,
                pDataSource, pMaxLen);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuDelete(NuArchive* pArchive)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_Delete(pArchive);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuDeleteRecord(NuArchive* pArchive, NuRecordIdx recordIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_DeleteRecord(pArchive, recordIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuDeleteThread(NuArchive* pArchive, NuThreadIdx threadIdx)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_DeleteThread(pArchive, threadIdx);
        Nu_ClearBusy(pArchive);
    }

    return err;
}


/*
 * ===========================================================================
 *      General interfaces
 * ===========================================================================
 */

NuError
NuClose(NuArchive* pArchive)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone) {
        Nu_SetBusy(pArchive);
        err = Nu_Close(pArchive);
        /* on success, pArchive has been freed */
        if (err != kNuErrNone)
            Nu_ClearBusy(pArchive);
    }

    return err;
}

NuError
NuGetMasterHeader(NuArchive* pArchive, const NuMasterHeader** ppMasterHeader)
{
    NuError err;

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone)
        err = Nu_GetMasterHeader(pArchive, ppMasterHeader);

    return err;
}

NuError
NuGetExtraData(NuArchive* pArchive, void** ppData)
{
    NuError err;

    if (ppData == nil)
        return kNuErrInvalidArg;
    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        *ppData = pArchive->extraData;

    return err;
}

NuError
NuSetExtraData(NuArchive* pArchive, void* pData)
{
    NuError err;

    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        pArchive->extraData = pData;

    return err;
}

NuError
NuGetValue(NuArchive* pArchive, NuValueID ident, NuValue* pValue)
{
    NuError err;

    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        return Nu_GetValue(pArchive, ident, pValue);

    return err;
}

NuError
NuSetValue(NuArchive* pArchive, NuValueID ident, NuValue value)
{
    NuError err;

    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        return Nu_SetValue(pArchive, ident, value);

    return err;
}

NuError
NuGetAttr(NuArchive* pArchive, NuAttrID ident, NuAttr* pAttr)
{
    NuError err;

    if ((err = Nu_PartiallyValidateNuArchive(pArchive)) == kNuErrNone)
        return Nu_GetAttr(pArchive, ident, pAttr);

    return err;
}

const char*
NuStrError(NuError err)
{
    return Nu_StrError(err);
}

NuError
NuDebugDumpArchive(NuArchive* pArchive)
{
#if defined(DEBUG_MSGS)
    /* skip validation checks for this one */
    Nu_DebugDumpAll(pArchive);
    return kNuErrNone;
#else
    /* function doesn't exist */
    return kNuErrGeneric;
#endif
}

NuError
NuGetVersion(long* pMajorVersion, long* pMinorVersion, long* pBugVersion,
    const char** ppBuildDate, const char** ppBuildFlags)
{
    return Nu_GetVersion(pMajorVersion, pMinorVersion, pBugVersion,
            ppBuildDate, ppBuildFlags);
}


/*
 * ===========================================================================
 *      Sources and Sinks
 * ===========================================================================
 */

NuError
NuCreateDataSourceForFile(NuThreadFormat threadFormat, short doClose,
    unsigned long otherLen, const char* pathname, short isFromRsrcFork,
    NuDataSource** ppDataSource)
{
    return Nu_DataSourceFile_New(threadFormat, (Boolean)(doClose != 0),
            otherLen, pathname, (Boolean)(isFromRsrcFork != 0), ppDataSource);
}

NuError
NuCreateDataSourceForFP(NuThreadFormat threadFormat, short doClose,
    unsigned long otherLen, FILE* fp, long offset, long length,
    NuDataSource** ppDataSource)
{
    return Nu_DataSourceFP_New(threadFormat, (Boolean)(doClose != 0),
            otherLen, fp, offset, length, ppDataSource);
}

NuError
NuCreateDataSourceForBuffer(NuThreadFormat threadFormat, short doClose,
    unsigned long otherLen, const unsigned char* buffer, long offset,
    long length, NuDataSource** ppDataSource)
{
    return Nu_DataSourceBuffer_New(threadFormat, (Boolean)(doClose != 0),
            otherLen, buffer, offset, length, ppDataSource);
}

NuError
NuFreeDataSource(NuDataSource* pDataSource)
{
    return Nu_DataSourceFree(pDataSource);
}

NuError
NuDataSourceSetRawCrc(NuDataSource* pDataSource, unsigned short crc)
{
    if (pDataSource == nil)
        return kNuErrInvalidArg;
    Nu_DataSourceSetRawCrc(pDataSource, crc);
    return kNuErrNone;
}

NuError
NuCreateDataSinkForFile(short doExpand, NuValue convertEOL,
    const char* pathname, char fssep, NuDataSink** ppDataSink)
{
    return Nu_DataSinkFile_New((Boolean)(doExpand != 0), convertEOL, pathname,
            fssep, ppDataSink);
}

NuError
NuCreateDataSinkForFP(short doExpand, NuValue convertEOL, FILE* fp,
    NuDataSink** ppDataSink)
{
    return Nu_DataSinkFP_New((Boolean)(doExpand != 0), convertEOL, fp,
            ppDataSink);
}

NuError
NuCreateDataSinkForBuffer(short doExpand, NuValue convertEOL,
    unsigned char* buffer, unsigned long bufLen, NuDataSink** ppDataSink)
{
    return Nu_DataSinkBuffer_New((Boolean)(doExpand != 0), convertEOL, buffer,
            bufLen, ppDataSink);
}

NuError
NuFreeDataSink(NuDataSink* pDataSink)
{
    return Nu_DataSinkFree(pDataSink);
}

NuError
NuDataSinkGetOutCount(NuDataSink* pDataSink, ulong* pOutCount)
{
    if (pDataSink == nil || pOutCount == nil)
        return kNuErrInvalidArg;

    *pOutCount = Nu_DataSinkGetOutCount(pDataSink);
    return kNuErrNone;
}


/*
 * ===========================================================================
 *      Non-archive operations
 * ===========================================================================
 */

void
NuRecordCopyAttr(NuRecordAttr* pRecordAttr, const NuRecord* pRecord)
{
    pRecordAttr->fileSysID = pRecord->recFileSysID;
    /*pRecordAttr->fileSysInfo = pRecord->recFileSysInfo;*/
    pRecordAttr->access = pRecord->recAccess;
    pRecordAttr->fileType = pRecord->recFileType;
    pRecordAttr->extraType = pRecord->recExtraType;
    pRecordAttr->createWhen = pRecord->recCreateWhen;
    pRecordAttr->modWhen = pRecord->recModWhen;
    pRecordAttr->archiveWhen = pRecord->recArchiveWhen;
}

NuError
NuRecordCopyThreads(const NuRecord* pNuRecord, NuThread** ppThreads)
{
    if (pNuRecord == nil || ppThreads == nil)
        return kNuErrInvalidArg;

    Assert(pNuRecord->pThreads != nil);

    *ppThreads = Nu_Malloc(nil, pNuRecord->recTotalThreads * sizeof(NuThread));
    if (*ppThreads == nil)
        return kNuErrMalloc;

    memcpy(*ppThreads, pNuRecord->pThreads,
        pNuRecord->recTotalThreads * sizeof(NuThread));

    return kNuErrNone;
}

unsigned long
NuRecordGetNumThreads(const NuRecord* pNuRecord)
{
    if (pNuRecord == nil)
        return -1;

    return pNuRecord->recTotalThreads;
}

const NuThread*
NuThreadGetByIdx(const NuThread* pNuThread, long idx)
{
    if (pNuThread == nil)
        return nil;
    return &pNuThread[idx];     /* can't range-check here */
}

short
NuIsPresizedThreadID(NuThreadID threadID)
{
    return Nu_IsPresizedThreadID(threadID);
}


/*
 * ===========================================================================
 *      Callback setters
 * ===========================================================================
 */

NuError
NuSetSelectionFilter(NuArchive* pArchive, NuCallback filterFunc)
{
    NuError err;

    /*Assert(!((ulong)filterFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone)
        pArchive->selectionFilterFunc = filterFunc;

    return err;
}

NuError
NuSetOutputPathnameFilter(NuArchive* pArchive, NuCallback filterFunc)
{
    NuError err;

    /*Assert(!((ulong)filterFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone)
        pArchive->outputPathnameFunc = filterFunc;

    return err;
}

NuError
NuSetProgressUpdater(NuArchive* pArchive, NuCallback updateFunc)
{
    NuError err;

    /*Assert(!((ulong)updateFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone)
        pArchive->progressUpdaterFunc = updateFunc;

    return err;
}

NuError
NuSetErrorHandler(NuArchive* pArchive, NuCallback errorFunc)
{
    NuError err;

    /*Assert(!((ulong)errorFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone)
        pArchive->errorHandlerFunc = errorFunc;

    return err;
}

NuError
NuSetErrorMessageHandler(NuArchive* pArchive, NuCallback messageHandlerFunc)
{
    NuError err;

    /*Assert(!((ulong)messageHandlerFunc % 4));*/

    if ((err = Nu_ValidateNuArchive(pArchive)) == kNuErrNone)
        pArchive->messageHandlerFunc = messageHandlerFunc;

    return err;
}

NuError
NuSetGlobalErrorMessageHandler(NuCallback messageHandlerFunc)
{
    /*Assert(!((ulong)messageHandlerFunc % 4));*/

    gNuGlobalErrorMessageHandler = messageHandlerFunc;
    return kNuErrNone;
}

