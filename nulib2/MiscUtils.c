/*
 * NuLib2
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING.
 *
 * Misc support functions.
 */
#include "NuLib2.h"


/*
 * Similar to perror(), but takes the error as an argument, and knows
 * about NufxLib errors as well as system errors.
 *
 * If "format" is NULL, just the error message itself is printed.
 */
void ReportError(NuError err, const char* format, ...)
{
    const char* msg;
    va_list args;

    Assert(format != NULL);

    va_start(args, format);

    /* print the message, if any */
    if (format != NULL) {
        fprintf(stderr, "%s: ERROR: ", gProgName);
        vfprintf(stderr, format, args);
    }

    /* print the error code data, if any */
    if (err == kNuErrNone)
        fprintf(stderr, "\n");
    else {
        if (format != NULL)
            fprintf(stderr, ": ");

        msg = NULL;
        if (err >= 0)
            msg = strerror(err);
        if (msg == NULL)
            msg = NuStrError(err);

        if (msg == NULL)
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
void* Malloc(size_t size)
{
    void* _result;

    Assert(size > 0);
    _result = malloc(size);
    if (_result == NULL) {
        ReportError(kNuErrMalloc, "malloc(%u) failed", (unsigned int) size);
        DebugAbort();   /* leave a core dump if we're built for it */
    }
    DebugFill(_result, size);
    return _result;
}

void* Calloc(size_t size)
{
    void* _cresult = Malloc(size);
    memset(_cresult, 0, size);
    return _cresult;
}

void* Realloc(void* ptr, size_t size)
{
    void* _result;

    Assert(ptr != NULL);     /* disallow this usage */
    Assert(size > 0);       /* disallow this usage */
    _result = realloc(ptr, size);
    if (_result == NULL) {
        ReportError(kNuErrMalloc, "realloc(%u) failed", (unsigned int) size);
        DebugAbort();   /* leave a core dump if we're built for it */
    }
    return _result;
}

void Free(void* ptr)
{
    if (ptr != NULL)
        free(ptr);
}
#endif

/*  
 * This gets called when a buffer DataSource is no longer needed.
 */
NuResult FreeCallback(NuArchive* pArchive, void* args)
{
    DBUG(("+++ free callback 0x%08lx\n", (long) args));
    Free(args);
    return kNuOK;
}

/*
 * Convert Mac OS Roman to Unicode.  The caller must free the string
 * returned.
 *
 * Returns NULL if stringMOR is NULL or if the conversion fails.
 */
UNICHAR* CopyMORToUNI(const char* stringMOR)
{
    size_t uniLen;
    UNICHAR* uniBuf;

    if (stringMOR == NULL) {
        return NULL;
    }

    uniLen = NuConvertMORToUNI(stringMOR, NULL, 0);
    if (uniLen == (size_t) -1) {
        return NULL;
    }
    uniBuf = (UNICHAR*) malloc(uniLen);
    NuConvertMORToUNI(stringMOR, uniBuf, uniLen);
    return uniBuf;
}

