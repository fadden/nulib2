NufxLib README, updated 2002/10/11
http://www.nulib.com/

See "COPYING-LIB" for distribution restrictions.


UNIX
====

Run the "configure" script.  Read through "INSTALL" if you haven't used
one of these before, especially if you want to use a specific compiler
or a particular set of compiler flags.

You can disable specific compression methods with "--disable-METHOD"
(run "sh ./configure --help" to see the possible options).  By default,
all methods are enabled except bzip2.

Run "make depend" if you have makedepend, and then type "make".  This will
build the library and all of the programs in the "samples" directory.
There are some useful programs in "samples", described in a README.txt
file there.  In particular, you should run samples/test-basic to verify
that things are more or less working.

If you want to install the library and header file into standard system
locations (usually /usr/local), run "make install".  To learn how to
specify different locations, read the INSTALL document.

There are some flags in "OPT" you may want to use.  The "autoconf" default
for @CFLAGS@ is "-g -O2".

-DNDEBUG
  Disable assert() calls and extra tests.  This will speed things up,
  but errors won't get caught until later on, making the root cause
  harder to locate.

-DDEBUG_MSGS
  Enable debug messages.  This increases the size of the executable,
  but shouldn't affect performance.  When errors occur, more output is
  produced.  The "debug dump" feature is enabled by this flag.

-DDEBUG_VERBOSE
  (Implicitly sets DEBUG_MSGS.)  Spray lots of debugging output.

If you want to do benchmarks, use "-O2 -DNDEBUG".  For pre-v1.0 sources,
setting -DNDEBUG is otherwise discouraged.  The recommended configuration
is "-g -O2 -DDEBUG_MSGS", so that verbose debug output is available when
errors occur.

The flags are stuffed into Version.c, so the application program can
examine and display the flags that were used to build the library.


BeOS
====

This works just like the UNIX version, but certain defaults have been
changed.  Running configure without arguments under BeOS is equivalent to:

    ./configure --prefix=/boot --includedir='${prefix}/develop/headers'
      --libdir='${exec_prefix}/home/config/lib' --mandir='/tmp'
      --bindir='${exec_prefix}/home/config/bin'

If you're using BeOS/PPC, it will also do:

    CC=cc CFLAGS='-proc 603 -opt full'


Win32
=====

If you're using an environment that supports "configure" scripts, such as
DJGPP, follow the UNIX instructions.

NufxLib has been tested with Microsoft Visual C++ 6.0.  To build NufxLib,
start up a DOS shell and run vcvars32.bat to set your environment.  Run:
    nmake -f makefile.msc
to build with debugging info, or
    nmake -f makefile.msc nodebug=1
to build optimized.

Once the library has been built, "cd samples" and run the same command there.
When it finishes, run "test-basic.exe".


Other Notes
===========

All of the source code is now formatted with spaces instead of tabs.

If you want to use the library in a multithreaded application, you should
define "USE_REENTRANT_CALLS" to tell it to use reentrant versions of
certain library calls.  This defines _REENTRANT, which causes Solaris to
add the appropriate goodies.  (Seems to me you'd always want this on, but
for some reason Solaris makes you take an extra step, so I'm not going to
define it by default.)


Legalese
========

NufxLib, a NuFX archive manipulation library
Copyright (C) 2000-2002 by Andy McFadden, All Rights Reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
MA 02111-1307, USA

