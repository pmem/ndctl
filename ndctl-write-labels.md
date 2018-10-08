---
title: ndctl
layout: pmdk
---

NAME
====

ndctl-write-labels - write data to the label area on a dimm

SYNOPSIS
========

>     ndctl write-labels <nmem> [-i <filename>]

DESCRIPTION
===========

The namespace label area is a small persistent partition of capacity
available on some NVDIMM devices. The label area is used to resolve
aliasing between *pmem* and *blk* capacity by delineating namespace
boundaries. Read data from the input filename, or stdin, and write it to
the given &lt;nmem&gt; device. Note that the device must not be active
in any region, otherwise the kernel will not allow write access to the
device’s label data area.

OPTIONS
=======

`<memory device(s)>`  
One or more *nmemX* device names. The keyword *all* can be specified to
operate on every dimm in the system, optionally filtered by bus id (see
--bus= option).

`-b; --bus=`  
Limit operation to memory devices (dimms) that are on the given bus.
Where *bus* can be a provider name or a bus id number.

`-v`  
Turn on verbose debug messages in the library (if ndctl was built with
logging and debug enabled).

`-i; --input`  
input file

SEE ALSO
========

[UEFI NVDIMM Label
Protocol](http://www.uefi.org/sites/default/files/resources/UEFI_Spec_2_7.pdf)
