---
title: ndctl
layout: pmdk
---

NAME
====

ndctl-enable-namespace - enable the given namespace(s)

SYNOPSIS
========

>     ndctl enable-namespace <namespace> [<options>]

THEORY OF OPERATION
===================

The capacity of an NVDIMM REGION (contiguous span of persistent memory) is accessed via one or more NAMESPACE devices. REGION is the Linux term for what ACPI and UEFI call a DIMM-interleave-set, or a system-physical-address-range that is striped (by the memory controller) across one or more memory modules.

The UEFI specification defines the *NVDIMM Label Protocol* as the combination of label area access methods and a data format for provisioning one or more NAMESPACE objects from a REGION. Note that label support is optional and if Linux does not detect the label capability it will automatically instantiate a "label-less" namespace per region. Examples of label-less namespaces are the ones created by the kernel’s *memmap=ss!nn* command line option (see the nvdimm wiki on kernel.org), or NVDIMMs without a valid *namespace index* in their label area.

A namespace can be provisioned to operate in one of 4 modes, *fsdax*, *devdax*, *sector*, and *raw*. Here are the expected usage models for these modes: - fsdax: Filesystem-DAX mode is the default mode of a namespace when specifying *ndctl create-namespace* with no options. It creates a block device (/dev/pmemX\[.Y\]) that supports the DAX capabilities of Linux filesystems (xfs and ext4 to date). DAX removes the page cache from the I/O path and allows mmap(2) to establish direct mappings to persistent memory media. The DAX capability enables workloads / working-sets that would exceed the capacity of the page cache to scale up to the capacity of persistent memory. Workloads that fit in page cache or perform bulk data transfers may not see benefit from DAX. When in doubt, pick this mode.

-   devdax: Device-DAX mode enables similar mmap(2) DAX mapping capabilities as Filesystem-DAX. However, instead of a block-device that can support a DAX-enabled filesystem, this mode emits a single character device file (/dev/daxX.Y). Use this mode to assign persistent memory to a virtual-machine, register persistent memory for RDMA, or when gigantic mappings are needed.

-   sector: Use this mode to host legacy filesystems that do not checksum metadata or applications that are not prepared for torn sectors after a crash. Expected usage for this mode is for small boot volumes. This mode is compatible with other operating systems.

-   raw: Raw mode is effectively just a memory disk that does not support DAX. Typically this indicates a namespace that was created by tooling or another operating system that did not know how to create a Linux *fsdax* or *devdax* mode namespace. This mode is compatible with other operating systems, but again, does not support DAX operation.

OPTIONS
=======

`<namespace>`  
A *namespaceX.Y* device name. The keyword *all* can be specified to carry out the operation on every namespace in the system, optionally filtered by region (see --region=option)

`-r; --region=`  
    A 'regionX' device name, or a region id number. The keyword 'all' can
    be specified to carry out the operation on every region in the system,
    optionally filtered by bus id (see --bus= option).

`-b; --bus=`  
Enforce that the operation only be carried on devices that are attached to the given bus. Where *bus* can be a provider name or a bus id number.

`-v; --verbose`  
Emit debug messages for the namespace operation

COPYRIGHT
=========

Copyright (c) 2016 - 2018, Intel Corporation. License GPLv2: GNU GPL version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you are free to change and redistribute it. There is NO WARRANTY, to the extent permitted by law.

SEE ALSO
========

[ndctl-disable-namespace](ndctl-disable-namespace.md)
