---
layout: page
---

# NAME

cxl-set-partition - set the partitioning between volatile and persistent
capacity on a CXL memdev

# SYNOPSIS

>     cxl set-partition <mem0> [ [<mem1>..<memN>] [<options>]

# DESCRIPTION

CXL devices may support both volatile and persistent memory capacity.
The amount of device capacity set aside for each type is typically
established at the factory, but some devices also allow for dynamic
re-partitioning.

Use this command to partition a device into volatile and persistent
capacity. The change in partitioning becomes the “next” configuration,
to become active on the next device reset.

Use "cxl list -m \<memdev\> -I" to examine the partitioning capabilities
of a device. A partition_alignment_size value of zero means there is no
available capacity and therefore the partitions cannot be changed.

Using this command to change the size of the persistent capacity shall
result in the loss of data stored.

# OPTIONS

\<memory device(s)\>  
A *memX* device name, or a memdev id number. Restrict the operation to
the specified memdev(s). The keyword *all* can be specified to indicate
the lack of any restriction.

`-S; --serial`  
Rather an a memdev id number, interpret the \<memdev\> argument(s) as a
list of serial numbers.

<!-- -->

`-b; --bus=`  
Restrict the operation to the specified bus.

`-t; --type=`  
Type of partition, *pmem* or *ram* (volatile), to modify. Default:
*pmem*

`-s; --size=`  
Size of the \<type\> partition in bytes. Size must align to the devices
alignment requirement. Use *cxl list -m \<memdev\> -I* to find
*partition_alignment_size*, or, use the --align option. Default: All
available capacity is assigned to \<type\>.

`-a; --align`  
Select this option to allow the automatic alignment of --size to meet
device alignment requirements. When using this option, specify the
minimum --size of the --type partition needed. When this option is
omitted, the command fails if --size is not properly aligned. Use *cxl
list -m \<memdev\> -I* to examine the partition_alignment_size.

`-v`  
Turn on verbose debug messages in the library (if libcxl was built with
logging and debug enabled).

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl-list](cxl-list), CXL-2.0 8.2.9.5.2
