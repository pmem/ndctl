---
title: ndctl
layout: pmdk
---

# NAME

cxl-free-dpa - release device-physical address space

# SYNOPSIS

>     cxl free-dpa <mem0> [<mem1>..<memN>] [<options>]

The CXL region provisioning process proceeds in multiple steps. One of
the steps is identifying and reserving the DPA span that each member of
the interleave-set (region) contributes in advance of attaching that
allocation to a region. For development, test, and debug purposes this
command is a helper to find the last allocated decoder on a device and
zero-out / free its DPA allocation.

# OPTIONS

\<memory device(s)>  
A *memX* device name, or a memdev id number. Restrict the operation to
the specified memdev(s). The keyword *all* can be specified to indicate
the lack of any restriction.

`-S; --serial`  
Rather an a memdev id number, interpret the \<memdev> argument(s) as a
list of serial numbers.

`-d; --decoder`  
Specify the decoder to free. The CXL specification mandates that DPA
must be released in the reverse order it was allocated. See
[cxl-reserve-dpa](cxl-reserve-dpa.md)

`-t; --type`  
Constrain the search for "last allocated decoder" to decoders targeting
the given partition.

`-f; --force`  
The kernel enforces CXL DPA ordering constraints on deallocation events,
and the tool anticipates those and fails operations that are expected to
fail without sending them to the kernel. For test purposes, continue to
attempt "expected to fail" operations to exercise the driver.

`-v`  
Turn on verbose debug messages in the library (if libcxl was built with
logging and debug enabled).

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl-reserve-dpa](cxl-reserve-dpa.md)
