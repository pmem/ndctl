---
title: ndctl
layout: pmdk
---

# NAME

cxl-read-labels - read out the label area on a CXL memdev

# SYNOPSIS

>     cxl read-labels <mem0> [<mem1>..<memN>] [<options>]

# DESCRIPTION

The region label area is a small persistent partition of capacity
available on some CXL memory devices. The label area is used to and
configure or determine the set of memory devices participating in
different interleave sets. This command dumps the raw binary data in a
memdev’s label area to stdout or a file. In the multi-memdev case the
data is concatenated.

# OPTIONS

\<memory device(s)>  
A *memX* device name, or a memdev id number. Restrict the operation to
the specified memdev(s). The keyword *all* can be specified to indicate
the lack of any restriction.

`-S; --serial`  
Rather an a memdev id number, interpret the \<memdev> argument(s) as a
list of serial numbers.

`-s; --size=`  
Limit the operation to the given number of bytes. A size of 0 indicates
to operate over the entire label capacity.

`-O; --offset=`  
Begin the operation at the given offset into the label area.

`-v`  
Turn on verbose debug messages in the library (if libcxl was built with
logging and debug enabled).

`-o; --output`  
output file

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl-write-labels](cxl-write-labels.md), [cxl-zero-labels](cxl-zero-labels.md), CXL-2.0
9.13.2
