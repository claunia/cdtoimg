CDToImg v1.01.
09 Oct 2006.
Written by Truman (My alias club.cdfreaks.com forum name).

Language and type: MS Visual Studio .NET 2002, Visual C++ v7, mixed C and C++,
Application type : Win32 console.

A program that reads an entire CD-ROM (mode 1 or mode 2 form 1) 2048 bytes per
sector and writes to a file - this would be an .ISO file. The CD must be CD-ROM
mode 1 or mode 2 and non-multisession. Note, it does not check if the CD is
valid.  TAO writing mode creates link blocks and this program does not detect
them so if encountered will be considered as a normal sector or unreadable.
Unreadable sectors are not supported and will terminate the process. Only up to
80 minutes reading is supported for the moment. Note that some drives will
ignore certain read speeds, e.g. my Plextor 755A can only be set to a minimum
of 4x read speed. On some drives, reading the last few sectors or ejecting the
disc when reading in 2048 byte/sector mode results in a hang (e.g.: LG CDROM
drive: HL-DT-ST CDROM GCR-8485B 1.05).

As always I do not take any responsibilities if this tool destroys your drive or
even anything else.

This code uses:
- Win32 IOCTL function with SCSI_PASS_THROUGH_DIRECT.
- The SCSI codes used in this source were taken from the draft documents MMC1.
  SPC1 and SAM1.
- MMC1 Read CD command (0xBE, CDB 12) to read sectors (2048 user bytes mode).
- Determine errors, retrieve and decode a few sense data.

You normally need the ntddscsi.h file from Microsoft DDK CD, but I shouldn't
distribute it, so instead I have written my own my_ntddscsi.h.

If you don't have windows.h some of the define constants are listed as comments.

This is a Win32 console program and only runs in a DOS prompt under Windows
NT4/2K/XP/2003 with appropriate user rights, i.e. you need to log in as
administrator.

cdtoimg <drive letter> <outputfile> [x read speed]
x speed is one of the following:
  - Enter CD x speed value.
  - Ommit or enter 0 to use currently set speed.
  - Enter m for max speed.

Example 1, to read from d drive, write to cd.iso file at 4x read speed:
cdtoimg d cd.iso 4

Example 2, to read from d drive, write to cd.iso file at maximum read speed:
cdtoimg d cd.iso m

Example 3, to read from d drive, write to cd.iso file at currently set read speed:
cdtoimg d cd.iso

Some notes about .ISO files
---------------------------

A while back ISO (International Standards Organisation) company published a standard
that described a file system for CD-ROM media called ISO9660, which most had adopted
and even extended. A problem stems from the recording programs needing to support CD
images and there were no standards for such files. Soon everyone was using .ISO CD image
file, which became popular because it couldn't belong to any company, because it was so
plain, like with the plain text file. When you read the user data (not including the
synchrization field, header, EDC and ECC) of 2048 bytes per sector from 00:02:00 to the
end of the 1st track of a CD-ROM disc and write them to a file you get an ISO file, and
it's where the name comes from, i.e. the CD-ROM file system structure. It has
limitations though (usual .ISO CD image file):

1. Must be CD-ROM format (mode 1 or mode 2 form 1).
2. Contains only 1 track.
3. No multi-session support.
4. Contains no sub-channel data.
5. No TOC data.
6. Starting address must be 00:02:00.
7. Contains no lead-in, 1st pregap, and lead-out.

I am only human, any errors in source code or descriptions you can mail me at:
trumanhi@hotmail.com

Hope it's been helpful to those wishing to learn CD programming.

Hardware tested to be working
-----------------------------

- LiteOn JLMS XJ-HD163
- LiteOn 52246S
- Plextor 8432T
- Plextor 755A
- LG GCR-8485B

History
-------
v1.01 - 22 Oct 2006
- Fixed some small silly bugs.

v1.00 - 09 Oct 2006
- First release.