/*
 * Nulib2
 * Copyright (C) 2000-2002 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 *
 * Misc support functions.
 */
#define __MiscUtils_c__
#include "Nulib2.h"


/*
 * Similar to perror(), but takes the error as an argument, and knows
 * about NufxLib errors as well as system errors.
 *
 * If "format" is nil, just the error message itself is printed.
 */
void
ReportError(NuError err, const char* format, ...)
{
    const char* msg;
    va_list args;

    Assert(format != nil);

    va_start(args, format);

    /* print the message, if any */
    if (format != nil) {
        fprintf(stderr, "%s: ERROR: ", gProgName);
        vfprintf(stderr, format, args);
    }

    /* print the error code data, if any */
    if (err == kNuErrNone)
        fprintf(stderr, "\n");
    else {
        if (format != nil)
            fprintf(stderr, ": ");

        msg = nil;
        if (err >= 0)
            msg = strerror(err);
        if (msg == nil)
            msg = NuStrError(err);

        if (msg == nil)
            fprintf(stderr, "(unknown err=%d)\n", err);
        else
            fprintf(stderr, "%s\n", msg);
    }

    va_end(args);
}


/*
 * Memory allocation wrappers.
 *
 * Under gcc these would be macros, but not all compilers can handle that.
 */

#ifndef USE_DMALLOC
void*
Malloc(size_t size)
{
    void* _result;

    Assert(size > 0);
    _result = malloc(size);
    if (_result == nil) {
        ReportError(kNuErrMalloc, "malloc(%u) failed", (uint) size);
        DebugAbort();   /* leave a core dump if we're built for it */
    }
    DebugFill(_result, size);
    return _result;
}

void*
Calloc(size_t size)
{
    void* _cresult = Malloc(size);
    memset(_cresult, 0, size);
    return _cresult;
}

void*
Realloc(void* ptr, size_t size)
{
    void* _result;

    Assert(ptr != nil);     /* disallow this usage */
    Assert(size > 0);       /* disallow this usage */
    _result = realloc(ptr, size);
    if (_result == nil) {
        ReportError(kNuErrMalloc, "realloc(%u) failed", (uint) size);
        DebugAbort();   /* leave a core dump if we're built for it */
    }
    return _result;
}

void
Free(void* ptr)
{
    if (ptr != nil)
        free(ptr);
}
#endif

