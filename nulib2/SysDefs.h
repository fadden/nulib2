/*
 * Nulib2
 * Copyright (C) 2000-2004 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 *
 * This was adapted from gzip's "tailor.h" and NuLib's "nudefs.h".
 */
#ifndef __SysDefs__
#define __SysDefs__

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef DEBUG_VERBOSE
# define DEBUG_MSGS
#endif

/* these should exist everywhere */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

/* basic Win32 stuff -- info-zip has much more complete defs */
#if defined(WIN32) || defined(MSDOS)
# define WINDOWS_LIKE

# ifndef HAVE_CONFIG_H
#  define HAVE_FCNTL_H
#  define HAVE_MALLOC_H
#  define HAVE_STDLIB_H
#  define HAVE_SYS_STAT_H
#  undef HAVE_SYS_TIME_H
#  define HAVE_SYS_TYPES_H
#  undef HAVE_UNISTD_H
#  undef HAVE_UTIME_H
#  define HAVE_SYS_UTIME_H
#  define HAVE_WINDOWS_H
#  define HAVE_FDOPEN
#  undef HAVE_FTRUNCATE
#  define HAVE_MEMMOVE
#  undef HAVE_MKSTEMP
#  define HAVE_MKTIME
#  define HAVE_SNPRINTF
#  undef HAVE_STRCASECMP
#  undef HAVE_STRNCASECMP
#  define HAVE_STRERROR
#  define HAVE_STRTOUL
#  define HAVE_VSNPRINTF
#  define SNPRINTF_DECLARED
#  define VSNPRINTF_DECLARED
#  define SPRINTF_RETURNS_INT
#  define uchar unsigned char
#  define ushort unsigned short
#  define uint unsigned int
#  define ulong unsigned long
#  define inline /*Visual C++6.0 can't inline ".c" files*/
#  define mode_t int
# endif

# include <io.h>
# include <direct.h>
# define FOPEN_WANTS_B
# define HAVE_CHSIZE
# define snprintf _snprintf
# define vsnprintf _vsnprintf

#endif


#ifdef HAVE_MALLOC_H
# include <malloc.h>
#endif
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#if defined(WINDOWS_LIKE)
# ifndef F_OK
#  define F_OK  00
# endif
# ifndef R_OK
#  define R_OK  04
# endif
#endif


#if defined(__unix__) || defined(__unix) || defined(__BEOS__) || \
    defined(__hpux) || defined(_AIX) || defined(__APPLE__)
# define UNIX_LIKE
#endif


#if defined(__MSDOS__) && !defined(MSDOS)
# define MSDOS
#endif

#if defined(__OS2__) && !defined(OS2)
# define OS2
#endif

#if defined(OS2) && defined(MSDOS) /* MS C under OS/2 */
# undef MSDOS
#endif

/* this ought to get trimmed down */
#ifdef MSDOS
#  ifndef __GNUC__
#    ifdef __TURBOC__
#      define NO_OFF_T
#      ifdef __BORLANDC__
#        define DIRENT
#      else
#        define NO_UTIME
#      endif
#    else /* MSC */
#      define HAVE_SYS_UTIME_H
#      define NO_UTIME_H
#    endif
#  endif
#  define PATH_SEP '\\'
#  define PATH_SEP2 '/'
#  define PATH_SEP3 ':'
#  ifdef MAX_PATH
#   define MAX_PATH_LEN MAX_PATH
#  else
#   define MAX_PATH_LEN 128
#  endif
#  define NO_MULTIPLE_DOTS
#  define MAX_EXT_CHARS 3
#  define NO_CHOWN
#  define PROTO
#  ifndef HAVE_CONFIG_H
#   define STDC_HEADERS
#  endif
#  define NO_SIZE_CHECK
#  define SYSTEM_DEFAULT_EOL "\r\n"
#endif

#ifdef WIN32 /* Windows 95/98/NT */
#  define HAVE_SYS_UTIME_H
#  define NO_UTIME_H
#  define PATH_SEP '\\'
#  define PATH_SEP2 '/'
#  define PATH_SEP3 ':'
#  ifdef MAX_PATH
#   define MAX_PATH_LEN MAX_PATH
#  else
#   define MAX_PATH_LEN 260
#  endif
#  define PROTO
#  define STDC_HEADERS
#  define SYSTEM_DEFAULT_EOL "\r\n"
#endif

#ifdef MACOS
#  define PATH_SEP ':'
#  define NO_CHOWN
#  define NO_UTIME
#  define SYSTEM_DEFAULT_EOL "\r"
#endif

#if defined(APW) || defined(__ORCAC__)
#  define __appleiigs__
#  pragma lint  -1
#  pragma memorymodel 1
#  pragma optimize 7
/*#  pragma debug 25 */
#  define PATH_SEP ':'
#  define SYSTEM_DEFAULT_EOL "\r"

#  ifdef GNO
#    define HAS_DIRENT
#  endif
#endif

#ifdef __GNUC__ /* this was missing from BeOS __MWERKS__, and probably others */
# define HAS__FUNCTION__
#endif

#if defined(__sun__) && !defined(__SVR4)
# include "SunOS4.h"
#endif

/* general defaults, mainly for UNIX */

#ifndef PATH_SEP
# define PATH_SEP '/'
#endif
#ifndef SYSTEM_DEFAULT_EOL
# define SYSTEM_DEFAULT_EOL "\n"
#endif
#ifndef MAX_PATH_LEN
# define MAX_PATH_LEN   1024
#endif

#endif /*__SysDefs__*/
