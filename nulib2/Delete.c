/*
 * Nulib2
 * Copyright (C) 2000-2002 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 *
 * Delete files from the archive.
 */
#include "Nulib2.h"


/*
 * Delete the specified files.
 *
 * This uses the "bulk" delete call, allowing the SelectionFilter callback
 * to do the matching against specified filenames.
 */
NuError
DoDelete(NulibState* pState)
{
    NuError err;
    NuArchive* pArchive = nil;

    Assert(pState != nil);

    err = OpenArchiveReadWrite(pState);
    if (err != kNuErrNone)
        goto bail;
    pArchive = NState_GetNuArchive(pState);
    Assert(pArchive != nil);

    NState_SetMatchCount(pState, 0);

    err = NuDelete(pArchive);
    if (err != kNuErrNone)
        goto bail;

    if (!NState_GetMatchCount(pState))
        printf("%s: no records matched\n", gProgName);

bail:
    if (pArchive != nil)
        (void) NuClose(pArchive);
    return err;
}

