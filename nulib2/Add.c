/*
 * NuLib2
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.
 *
 * Add files to or update files in the archive.
 */
#include "NuLib2.h"

static NuError AddToArchive(NulibState* pState, NuArchive* pArchive);


/*
 * Add the specified files to a new or existing archive.
 */
NuError DoAdd(NulibState* pState)
{
    NuError err;
    NuArchive* pArchive = NULL;
    uint32_t flushStatus;

    Assert(pState != NULL);

    err = OpenArchiveReadWrite(pState);
    if (err != kNuErrNone)
        goto bail;

    pArchive = NState_GetNuArchive(pState);
    Assert(pArchive != NULL);

    NState_SetMatchCount(pState, 0);

    /* tell them about the list of files */
    err = AddToArchive(pState, pArchive);
    if (err != kNuErrNone)
        goto bail;

    /*(void)NuDebugDumpArchive(pArchive);*/

    if (!NState_GetMatchCount(pState))
        printf("%s: no records matched\n", gProgName);

bail:
    if (pArchive != NULL) {
        NuError err2;

        #if 0
        if (err != kNuErrNone) {
            printf("Attempting to flush changes in spite of errors...\n");
            err = kNuErrNone;
        }
        #endif

        if (err == kNuErrNone) {
            err = NuFlush(pArchive, &flushStatus);
            if (err != kNuErrNone) {
                if (flushStatus & kNuFlushSucceeded) {
                    ReportError(err,
                        "Changes were successfully written, but something "
                        "failed afterward");
                } else {
                    ReportError(err,
                        "Unable to flush archive changes (status=0x%04x)",
                        flushStatus);
                }
                NuAbort(pArchive);
            }
        } else {
            NuAbort(pArchive);
        }

        err2 = NuClose(pArchive);
        Assert(err2 == kNuErrNone);
    }
    return err;
}


/*
 * Add the requested files to the specified archive.
 *
 * This just results in NuAddFile calls; the deferred write operation
 * isn't initiated.
 */
static NuError AddToArchive(NulibState* pState, NuArchive* pArchive)
{
    NuError err = kNuErrNone;
    char* const* pSpec;
    int i;

    Assert(pState != NULL);
    Assert(pArchive != NULL);

    if (!NState_GetFilespecCount(pState)) {
        err = kNuErrSyntax;
        ReportError(err, "no files were specified");
    }

    pSpec = NState_GetFilespecPointer(pState);
    for (i = NState_GetFilespecCount(pState); i > 0; i--, pSpec++) {
        err = AddFile(pState, pArchive, *pSpec);
        if (err != kNuErrNone)
            goto bail;
    }

bail:
    return err;
}

