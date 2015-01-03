NuLib2 README, updated 2015/01/03
http://www.nulib.com/


To build NuLib2, you will also need a copy of NufxLib.  This should have come
in the same distribution file.  Build the NufxLib library first.


UNIX
====

Make sure that the "NUFXSRCDIR" define in Makefile.in points to the correct
directory, or that the library has been installed in a standard location
such as /usr/local/lib/.  If you received NuLib2 and NufxLib in a single
".tar.gz" file, the variable is already set correctly.  The makefile will
look in $(NUFXSRCDIR) first, /usr/local/lib second.

Run the "configure" script.  Read through "INSTALL" if you haven't used
one of these before, especially if you want to use a specific compiler
or a particular set of compiler flags.

If you have disabled deflate or enabled bzip2 support in libnufx.a, you
will need to provide the same --enable-METHOD or --disable-METHOD flag
to configure here.  If you're using shared libraries then the link
dependencies are taken care of and you don't need to do anything.

Run "make depend" if you have makedepend, and then type "make".
This should leave you with an executable called "nulib2".  If you like,
"make install" will copy files into your install directory, usually
/usr/local/bin/ and /usr/local/man/.  You can change this by using
"./configure --prefix=directory".

You may want to fiddle with the "OPT" setting in Makefile to enable or
disable optimizations and assertions.  Because almost all of the hard
work is done by NufxLib, turning compiler optimizations on in NuLib2 has
little impact on performance.


A man page for nulib2 is in "nulib2.1", which you can format for viewing
with "nroff -man nulib2.1".  A full manual for NuLib2 is available from
the www.nulib.com web site.


BeOS
====

This works just like the UNIX version, but certain defaults have been
changed.  Running configure without arguments under BeOS is equivalent to:

    ./configure --prefix=/boot --includedir='${prefix}/develop/headers'
      --libdir='${exec_prefix}/home/config/lib' --mandir='/tmp'
      --bindir='${exec_prefix}/home/config/bin' 
      
If you're using BeOS/PPC, it will also do:

    CC=cc CFLAGS='-proc 603 -opt full'
    

Mac OS X
========

This works just like the UNIX version.  You'll see some warnings due to some
namespace collisions between nufxlib and Carbon, but everything will work
fine.  The Carbon framework is used to enable support for filetypes and
resource forks.


Win32
=====

If you're using an environment that supports "configure" scripts, such as
DJGPP, follow the UNIX instructions.

NuLib2 has been tested with Microsoft Visual C++ 12 (Visual Studio 2013).
To build NuLib2, run the "Visual Studio 2013 x86 Native Tools Command
Prompt" shortcut to get a shell.  Change to the nulib2 directory, then:

    nmake -f makefile.msc

If you want to have zlib support enabled, you will need to have zlib.lib
copied into the directory.  See "makefile.msc" for more details.


Other Notes
===========

Fun benchmark of the day:

Time to compress 1525 files, totaling 19942152 bytes, on an Apple IIgs
with an 8MHz ZipGS accelerator and Apple HS SCSI card, running System
6.0.1, from a 20MB ProDOS partition to a 13.9MB archive on an HFS volume,
with GS/ShrinkIt 1.1: about 40 minutes.

Time to compress the same files, on a 128MB 500MHz Pentium-III running
Red Hat Linux 6.0, with NuLib2 v0.3: about six seconds.



Here's a nifty way to evaluate GSHK vs NuLib2 (as well as Linux NuLib2
vs Win32 NuLib2):

 - Archive a whole bunch of files from a ProDOS volume with GS/ShrinkIt.
   I used a 20MB partition, which resulted in a 14MB archive.  Transfer
   the archive to a machine running NuLib2 (perhaps a Linux system).
 - Create a new subdirectory, cd into it, and extract the entire archive
   with "nulib2 xe ../foo.shk".
 - Now create a new archive with all of the files, using
   "nulib2 aer ../new.shk *".
 - Change back to the directory above, and use "nulib2 v" to see what's
   in them, e.g. "nulib2 v foo.shk > out.orig" and
   "nulib2 v new.shk > out.new".
 - Edit both of the "out" files with vi.  Do a global search-and-replace
   in both files to set the file dates to be the same.  I used
   ":%s/..-...-.. ..:../01-Jan-00 00:00/".  This is necessary because,
   like ShrinkIt, NuLib displays the date on which the files were archived,
   not when they were last modified.
 - Sort both files, with ":%!sort".  This is necessary because you
   added the files with '*' up above, so the NuLib2-created archive
   has the top-level files alphabetized.
 - Quit out of vi.  Diff the two files.

I did this for a 20MB hard drive partition with 1500 files on it.
The only discrepancies (accounting for a total difference of 116 bytes)
were a zero-byte "Kangaroo.data" file that GSHK stored improperly and some
semi-random GSHK behavior that I can't mimic.  When the "Mimic ShrinkIt"
flag is disabled, the resulting archive is 13K smaller.


The largest archive I've tried had 4700 files for a total of 76MB
(compressed down to 47MB), and contained the entire contents of four 20MB
ProDOS hard drive partitions.  NuLib2 under Linux handled it without
breaking a sweat.


Legalese
========

NuLib2, a NuFX and Binary II archive application.
Copyright (C) 2000-2014 by Andy McFadden, All Rights Reserved.

See COPYING for license.

