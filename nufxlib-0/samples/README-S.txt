NufxLib "samples" README

This directory contains some test programs and useful sample code.


test-basic
==========

Basic tests.  Run this to verify that things are working.


exerciser
=========

This program allows you to exercise all of NufxLib's basic functions.
Run it without arguments and hit "?" for a list of commands.

If you think you have found a bug in NufxLib, you can use this to narrow
it down to a repeatable case.


imgconv
=======

A 2IMG disk image converter.  You can convert ".2MG" files to ShrinkIt
disk archives, and ShrinkIt disk archives to 2IMG format.  imgconv uses
a creator type of "NFXL".

You can use it like this:

  % imgconv file.shk file.2mg
or
  % imgconv file.2mg file.shk

It figures out what to do based on the filename.  It will recognize ".sdk"
as a ShrinkIt archive.

Limitations: works for DOS-ordered and ProDOS-ordered 2MG images, but
not for raw nibble images.  Converting from .shk only works if the first
record in the archive is a disk image; you don't get to pick the one you
want from an archive with several in it.


launder
=======

Run an archive through the laundry.  This copies the entire contents of
an archive thread-by-thread, reconstructing it such that the data
matches the original even if the archive contents don't (e.g. records
are updated to version 3, files may be recompressed with LZW/2, option
lists are stripped out, etc).

The basic usage is:

  % launder [-crfa] infile.shk outfile.shk

The flags are:

  -c  Just copy threads rather than recompressing them
  -r  Add threads in reverse order
  -f  Call NuFlush after every record
  -a  Call NuAbort after every record, then re-do the record and call NuFlush
  -t  Write to temp file, instead of writing directly into outfile.shk

If you use the "-c" flag with an archive created by P8 ShrinkIt or NuLib,
the laundered archive may have CRC failures.  This is because "launder"
creates version 3 records, which are expected to have a valid CRC in the
thread header.  The only way to compute the CRC is to uncompress the data,
which "launder" doesn't do when "-c" is set.  The data itself is fine,
it's just the thread CRC that's wrong (if the data were hosed, the LZW/1
CRC would be bad too).  "launder" will issue a warning when it detects
this situation.

If you find that you're running out of memory on very large archives, you
can reduce the memory requirements by specifying the "-f" flag.


test-simple
===========

Simple test program.  Give it the name of an archive, and it will display
the contents.


test-extract
============

Simple test program.  Give it the name of an archive, and it will write
all filename threads into "out.buf", "out.fp", and "out.file" using three
different kinds of NuDataSinks.

