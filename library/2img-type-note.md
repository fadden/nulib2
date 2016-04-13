Universal Disk Image file
=========================

- File Type: $E0
- Auxiliary Type: $0130
- Full Name: Universal Disk Image file
- Short Name: 2IMG

Files of this type and auxiliary type contain Universal Disk Image format disk images.

---

Files of this type and auxtype contain data in Universal Disk Image format. This file format, a recent standard developed by cooperation among Apple II emulator authors, is used to represent an entire Apple II-compatible disk in a data file. It's an extremely easy-to-use and flexible format that's compatible with most currently-supported Apple II emulation products.

Univeral Disk Image files (also known as '2IMG' or '2MG', from the Macintosh creator code and filename extensions typically used for these files) consist of four chunks: the header, the data chunk, the comment, and the creator-specific data chunk.

### The Universal Disk Image header ###

The header can be described using the following C structure. All values are little-endian (Apple II byte ordering):

offset | size | description
------ | ---- | -----------
+$000  | Long |  The integer constant '2IMG'. This integer should be little-endian, so on the Apple IIgs, this is equivalent to the four characters 'GMI2'; in ORCA/C 2.1, you can use the integer constant '2IMG'.
+$004  | Long |  A four-character tag identifying the application that created the file.
+$008  | Word |  The length of this header, in bytes. Should be 52.
+$00A  | Word |  The version number of the image file format. Should be 1.
+$00C  | Long |  The image format. See table below.
+$010  | Long |  Flags. See table below.
+$014  | Long |  The number of 512-byte blocks in the disk image. This value should be zero unless the image format is 1 (ProDOS order).
+$018  | Long |  Offset to the first byte of the first block of the disk in the image file, from the beginning of the file. The disk data must come before the comment and creator-specific chunks.
+$01C  | Long |  Length of the disk data in bytes. For a ProDOS image, this would be the number of blocks * 512.
+$020  | Long |  Offset to the first byte of the image comment, or zero if there's no comment. The comment must come after the data chunk, but before the creator-specific chunk. The comment, if it exists, should be raw ASCII text; no length byte or C-style null terminator byte is required (that's what the next field is for).
+$024  | Long |  Length of the comment chunk. Zero if there's no comment.
+$028  | Long |  Offset to the first byte of the creator-specific data chunk, or zero if there is none.  This chunk must come last.
+$02C  | Long |  Length of the creator-specific chunk; zero if there is no creator-specific data.
+$030  | 16 bytes |  Reserved space; this pads the header to 64 bytes. These values must all be zero.


### Creator Codes ###

If the file is created by a Macintosh application, this should be the creator code of the creating application; otherwise, this should be a unique ID specific to the creating application.

The following creator codes are already in use by various applications:

- ASIMOV2               '!nfc'
- Bernie ][ the Rescue  'B2TR'
- Catakig               'CTKG'
- Ciderpress            'CdrP'
- Sheppy's ImageMaker   'ShIm'
- Sweet 16              'WOOF'
- XGS                   'XGS!'

If you replace the creator type, you should also convert or remove the contents of the creator-specific data chunk.


### The Image Format field ###

The image format is indicated as one of the following values:

- DOS 3.3 Order   0
- ProDOS Order    1
- Nibblized data  2


### The Flags field ###

The flags field contains bit flags that specify details about the disk image. Bits not defined here must be zero.

Bit | Description
--- | -----------
31  | If this bit is set, the disk image is locked and no changes should be permitted to the disk data it contains. This is used by emulators to provide support for write-protecting disk images.
8   | If this bit is set, the low byte of the flags field contains the DOS 3.3 volume number of the disk image. This bit should only be set for DOS 3.3 disk images. If the disk is DOS 3.3 format (the image format is 0), and this bit is 0, volume number 254 is assumed.
7-0 | The DOS 3.3 volume number, from 0-254, if bit 8 is set. Otherwise these bits should be 0.


### Authors ###

The format was developed primarily by Henrik Gudat.  A [newsgroup post]
(https://groups.google.com/d/msg/comp.emulators.apple2/xhKfAlrMBVU/EkJNLOCweeQJ)
from December 1997 has a basic explanation, and a page from
[www.magnet.ch](http://web.archive.org/web/19981206023530/http://www.magnet.ch/emutech/Tech/),
archived December 1998, describes the format in detail.

A file-type-note-format version was created by Eric Shepherd in March 1999.

This doc, formatted for github-style Markdown, was created by Andy McFadden
in December 2014.

