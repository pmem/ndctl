---
title: ndctl
layout: pmdk
---

NAME
====

daxctl-io - Perform I/O on Device-DAX devices or zero a Device-DAX device.

SYNOPSIS
========

>     daxctl io [<options>]

There must be a Device-DAX device involved whether as the input or the output device. Read from a Device-DAX device and write to a file descriptor, or another Device-DAX device. Write to a Device-DAX device from a file descriptor or another Device-DAX device.

No length specified will default to input file/device length. If input is a special char file then length will be the output file/device length.

No input will default to stdin. No output will default to stdout.

For a Device-DAX device, attempts to clear badblocks within range of writes will be performed.

EXAMPLE
=======

>     # daxctl io --zero /dev/dax1.0

\# daxctl io --input=/dev/dax1.0 --output=/home/myfile --len=2M --seek=4096

\# cat /dev/zero | daxctl io --output=/dev/dax1.0

\# daxctl io --input=/dev/zero --output=/dev/dax1.0 --skip=4096

OPTIONS
=======

-i; --input=  
Input device or file to read from.

-o; --output=  
Output device or file to write to.

-z; --zero  
Zero the output device for *len* size. Or the entire device if no length was provided. The output device must be a Device DAX device.

-l; --len  
The length in bytes to perform the I/O. The following suffixes are supported to make passing in size easier for kibi, mebi, gibi, and tebi bytes: k/K,m/M,g/G,t/T. i.e. 20m - 20 Mebibytes

-s; --seek  
The number of bytes to skip over on the output before performing a write.

-k; --skip  
The number of bytes to skip over on the input before performing a read.

COPYRIGHT
=========

Copyright (c) 2017, Intel Corporation. License GPLv2: GNU GPL version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you are free to change and redistribute it. There is NO WARRANTY, to the extent permitted by law.
