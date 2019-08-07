---
title: ndctl
layout: pmdk
---

NAME
====

ndctl-read-labels - read out the label area on a dimm or set of dimms

SYNOPSIS
========

>     ndctl read-labels <nmem0> [<nmem1>..<nmemN>] [<options>]

DESCRIPTION
===========

The namespace label area is a small persistent partition of capacity
available on some NVDIMM devices. The label area is used to resolve
aliasing between *pmem* and *blk* capacity by delineating namespace
boundaries. This command dumps the raw binary data in a dimm’s label
area to stdout or a file. In the multi-dimm case the data is
concatenated.

OPTIONS
=======

`<memory device(s)>`  
One or more *nmemX* device names. The keyword *all* can be specified to
operate on every dimm in the system, optionally filtered by bus id (see
--bus= option).

`-s; --size=`  
Limit the operation to the given number of bytes. A size of 0 indicates
to operate over the entire label capacity.

`-O; --offset=`  
Begin the operation at the given offset into the label area.

`-b; --bus=`  
Limit operation to memory devices (dimms) that are on the given bus.
Where *bus* can be a provider name or a bus id number.

`-v`  
Turn on verbose debug messages in the library (if ndctl was built with
logging and debug enabled).

`-I; --index`  
Limit the span of the label operation to just the index-block area. This
is useful to determine if the dimm label area is initialized. Note that
this option and --size/--offset are mutually exclusive.

`-o; --output`  
output file

`-j; --json`  
parse the label data into json assuming the *NVDIMM Namespace
Specification* format.

`-u; --human`  
enable json output and convert number formats to human readable strings,
for example show the size in terms of "KB", "MB", "GB", etc instead of a
signed 64-bit numbers per the JSON interchange format (implies --json).

COPYRIGHT
=========

Copyright (c) 2016 - 2019, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

SEE ALSO
========

[UEFI NVDIMM Label
Protocol](http://www.uefi.org/sites/default/files/resources/UEFI_Spec_2_7.pdf)
