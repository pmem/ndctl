---
title: ndctl
layout: pmdk
---

# NAME

cxl-zero-labels - zero out the label area on a set of memdevs

# SYNOPSIS

>     cxl zero-labels <mem0> [<mem1>..<memN>] [<options>]

# DESCRIPTION

The region label area is a small persistent partition of capacity
available on some CXL memory devices. The label area is used to and
configure or determine the set of memory devices participating in
different interleave sets. This command resets the device to its default
state by deleting all labels.

# OPTIONS

\<memory device(s)>  
A *memX* device name, or a memdev id number. Restrict the operation to
the specified memdev(s). The keyword *all* can be specified to indicate
the lack of any restriction.

`-s; --size=`  
Limit the operation to the given number of bytes. A size of 0 indicates
to operate over the entire label capacity.

`-O; --offset=`  
Begin the operation at the given offset into the label area.

`-v`  
Turn on verbose debug messages in the library (if libcxl was built with
logging and debug enabled).

# COPYRIGHT

Copyright © 2016 - 2020, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl-read-labels](cxl-read-labels.md), [cxl-write-labels](cxl-write-labels.md), CXL-2.0
9.13.2
