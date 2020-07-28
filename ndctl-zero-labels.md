---
title: ndctl
layout: pmdk
---

NAME
====

ndctl-zero-labels - zero out the label area on a dimm or set of dimms

SYNOPSIS
========

>     ndctl zero-labels <nmem0> [<nmem1>..<nmemN>] [<options>]

DESCRIPTION
===========

The namespace label area is a small persistent partition of capacity
available on some NVDIMM devices. The label area is used to resolve
aliasing between *pmem* and *blk* capacity by delineating namespace
boundaries. This command resets the device to its default state by
deleting all labels.

OPTIONS
=======

\<memory device(s)\>  
A *nmemX* device name, or a dimm id number. Restrict the operation to
the specified dimm(s). The keyword *all* can be specified to indicate
the lack of any restriction, however this is the same as not supplying a
--dimm option at all.

`-s; --size=`  
Limit the operation to the given number of bytes. A size of 0 indicates
to operate over the entire label capacity.

`-O; --offset=`  
Begin the operation at the given offset into the label area.

`-b; --bus=`  
A bus id number, or a provider string (e.g. "ACPI.NFIT"). Restrict the
operation to the specified bus(es). The keyword *all* can be specified
to indicate the lack of any restriction, however this is the same as not
supplying a --bus option at all.

`-v`  
Turn on verbose debug messages in the library (if ndctl was built with
logging and debug enabled).

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
