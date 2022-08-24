---
title: ndctl
layout: pmdk
---

# NAME

ndctl-init-labels - initialize the label data area on a dimm or set of
dimms

# SYNOPSIS

>     ndctl init-labels <nmem0> [<nmem1>..<nmemN>] [<options>]

# DESCRIPTION

The namespace label area is a small persistent partition of capacity
available on some NVDIMM devices. The label area is used to provision
one, or more, namespaces from regions. Starting with v4.10 the kernel
will honor labels for sub-dividing PMEM if all the DIMMs in an
interleave set / region have a valid namespace index block.

This command can be used to initialize the namespace index block if it
is missing or reinitialize it if it is damaged. Note that
reinitialization effectively destroys all existing namespace labels on
the DIMM.

# EXAMPLE

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

# OPTIONS

\<memory device(s)>  
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

`-f; --force`  
Force initialization of the label space even if there appears to be an
existing / valid namespace index. Warning, this will destroy all defined
namespaces on the dimm.

`-V; --label-version`  
Initialize with a specific version of labels from the namespace label
specification. Defaults to 1.1

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[ndctl-create-namespace](ndctl-create-namespace.md) , [UEFI NVDIMM Label
Protocol](http://www.uefi.org/sites/default/files/resources/UEFI_Spec_2_7.pdf)
