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

The namespace label area is a small persistent partition of capacity available on some NVDIMM devices. The label area is used to resolve aliasing between *pmem* and *blk* capacity by delineating namespace boundaries. This command resets the device to its default state by deleting all labels.

OPTIONS
=======

`<memory device(s)&gt;…  
One or more *nmemX* device names. The keyword *all* can be specified to operate on every dimm in the system, optionally filtered by bus id (see --bus= option).

`-b; --bus=`  
Limit operation to memory devices (dimms) that are on the given bus. Where *bus* can be a provider name or a bus id number.

`-v`  
Turn on verbose debug messages in the library (if ndctl was built with logging and debug enabled).

COPYRIGHT
=========

Copyright (c) 2016 - 2017, Intel Corporation. License GPLv2: GNU GPL version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you are free to change and redistribute it. There is NO WARRANTY, to the extent permitted by law.

SEE ALSO
========

[UEFI NVDIMM Label Protocol](http://www.uefi.org/sites/default/files/resources/UEFI_Spec_2_7.pdf)
