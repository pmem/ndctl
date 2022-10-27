---
layout: page
---

# NAME

cxl-reserve-dpa - allocate device-physical address space

# SYNOPSIS

>     cxl reserve-dpa <mem0> [<mem1>..<memN>] [<options>]

The CXL region provisioning process proceeds in multiple steps. One of
the steps is identifying and reserving the DPA span that each member of
the interleave-set (region) contributes in advance of attaching that
allocation to a region. For development, test, and debug purposes this
command is a helper to find the next available decoder on endpoint
(memdev) and mark a span of DPA as busy.

# OPTIONS

\<memory device(s)>  
A *memX* device name, or a memdev id number. Restrict the operation to
the specified memdev(s). The keyword *all* can be specified to indicate
the lack of any restriction.

`-S; --serial`  
Rather an a memdev id number, interpret the \<memdev> argument(s) as a
list of serial numbers.

`-d; --decoder`  
Specify the decoder to attempt the allocation. The CXL specification
mandates that allocations must be ordered by DPA and decoder instance.
I.e. the lowest DPA allocation on the device is covered by decoder0, and
the last / highest DPA allocation is covered by the last decoder. This
ordering is enforced by the kernel. By default the tool picks the *next
available* decoder.

`-t; --type`  
Select the partition for the allocation. CXL devices implement a
partition that divdes *ram* and *pmem* capacity, where *pmem* capacity
consumes the higher DPA capacity above the partition boundary. The type
defaults to *pmem*. Note that given CXL DPA allocation constraints, once
any *pmem* allocation is established then all remaining *ram* capacity
becomes reserved (skipped).

`-f; --force`  
The kernel enforces CXL DPA allocation ordering constraints, and the
tool anticipates those and fails operations that are expected to fail
without sending them to the kernel. For test purposes, continue to
attempt "expected to fail" operations to exercise the driver.

`-s; --size`  
Specify the size of the allocation. This option supports the suffixes
"k" or "K" for KiB, "m" or "M" for MiB, "g" or "G" for GiB and "t" or
"T" for TiB. This defaults to "all available capacity of the specified
type".

`-v`  
Turn on verbose debug messages in the library (if libcxl was built with
logging and debug enabled).

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl-free-dpa](cxl-free-dpa)
