---
title: ndctl
layout: pmdk
---

NAME
====

ndctl-init-labels - initialize the label data area on a dimm or set of
dimms

SYNOPSIS
========

>     ndctl init-labels <nmem0> [<nmem1>..<nmemN>] [<options>]

DESCRIPTION
===========

The namespace label area is a small persistent partition of capacity
available on some NVDIMM devices. The label area is used to resolve
aliasing between *pmem* and *blk* capacity by delineating namespace
boundaries. By default, and in kernels prior to v4.10, the kernel only
honors labels when a DIMM aliases PMEM and BLK capacity. Starting with
v4.10 the kernel will honor labels for sub-dividing PMEM if all the
DIMMs in an interleave set / region have a valid namespace index block.

This command can be used to initialize the namespace index block if it
is missing or reinitialize it if it is damaged. Note that
reinitialization effectively destroys all existing namespace labels on
the DIMM.

EXAMPLE
=======

Find the DIMMs that comprise a given region:

    # ndctl list -RD --region=region1
    {
      "dimms":[
        {
          "dev":"nmem0",
          "id":"8680-56341200"
        }
      ],
      "regions":[
        {
          "dev":"region1",
          "size":268435456,
          "available_size":0,
          "type":"pmem",
          "mappings":[
            {
              "dimm":"nmem0",
              "offset":13958643712,
              "length":268435456
            }
          ]
        }
      ]
    }

Disable that region so the DIMM label area can be written from
userspace:

    # ndctl disable-region region1

Initialize labels:

    # ndctl init-labels nmem0

Re-enable the region:

    # ndctl enable-region region1

Create a namespace in that region:

    # ndctl create-namespace --region=region1

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

`-f; --force`  
Force initialization of the label space even if there appears to be an
existing / valid namespace index. Warning, this will destroy all defined
namespaces on the dimm.

`-V; --label-version`  
Initialize with a specific version of labels from the namespace label
specification. Defaults to 1.1

COPYRIGHT
=========

Copyright (c) 2016 - 2019, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

SEE ALSO
========

[ndctl-create-namespace](ndctl-create-namespace.md) , [UEFI NVDIMM Label
Protocol](http://www.uefi.org/sites/default/files/resources/UEFI_Spec_2_7.pdf)
