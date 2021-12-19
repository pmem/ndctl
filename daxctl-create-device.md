---
title: ndctl
layout: pmdk
---

# NAME

daxctl-create-device - Create a devdax device

# SYNOPSIS

>     daxctl create-device [<options>]

# EXAMPLES

-   Creates dax0.1 with 4G of size

<!-- -->

    # daxctl create-device -s 4G
    [
      {
        "chardev":"dax0.1",
        "size":4294967296,
        "target_node":0,
        "mode":"devdax"
      }
    ]

-   Creates devices with fully available size on all regions

<!-- -->

    # daxctl create-device -u
    [
      {
        "chardev":"dax0.1",
        "size":"15.63 GiB (16.78 GB)",
        "target_node":0,
        "mode":"devdax"
      },
      {
        "chardev":"dax1.1",
        "size":"15.63 GiB (16.78 GB)",
        "target_node":1,
        "mode":"devdax"
      }
    ]

-   Creates dax0.1 with fully available size on region id 0

<!-- -->

    # daxctl create-device -r 0 -u
    {
      "chardev":"dax0.1",
      "size":"15.63 GiB (16.78 GB)",
      "target_node":0,
      "mode":"devdax"
    }

# DESCRIPTION

Creates dax device in *devdax* mode in dynamic regions. The resultant
can also be convereted to the *system-ram* mode which arranges for the
dax range to be hot-plugged into the system as regular memory.

*daxctl create-device* expects that the BIOS or kernel defines a range
in the EFI memory map with EFI_MEMORY_SP. The resultant ranges mean that
it’s 100% capacity is reserved for applications.

# OPTIONS

`-r; --region=`  
Restrict the operation to devices belonging to the specified region(s).
A device-dax region is a contiguous range of memory that hosts one or
more /dev/daxX.Y devices, where X is the region id and Y is the device
instance id.

`-s; --size=`  
For regions that support dax device cretion, set the device size in
bytes. Otherwise it defaults to the maximum size specified by region.
This option supports the suffixes "k" or "K" for KiB, "m" or "M" for
MiB, "g" or "G" for GiB and "t" or "T" for TiB.

    The size must be a multiple of the region alignment.

`-a; --align`  
Applications that want to establish dax memory mappings with page table
entries greater than system base page size (4K on x86) need a device
that is sufficiently aligned. This defaults to 2M. Note that "devdax"
mode enforces all mappings to be aligned to this value, i.e. it fails
unaligned mapping attempts.

`--input`  
Applications that want to select ranges assigned to a device-dax
instance, or wanting to establish previously created devices, can pass
an input JSON file. The file option lets a user pass a JSON object
similar to the one listed with "daxctl list".

    The device name is not re-created, but if a "chardev" is passed in
    the JSON file, it will use that to get the region id.

    Note that the JSON content in the file cannot be an array of
    JSON objects but rather a single JSON object i.e. without the
    array enclosing brackets.

<!-- -->

`-u; --human`  
By default the command will output machine-friendly raw-integer data.
Instead, with this flag, numbers representing storage size will be
formatted as human readable strings with units, other fields are
converted to hexadecimal strings.

<!-- -->

`-v; --verbose`  
Emit more debug messages

# COPYRIGHT

Copyright © 2016 - 2020, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[daxctl-list](daxctl-list.md),[daxctl-reconfigure-device](daxctl-reconfigure-device.md),[daxctl-destroy-device](daxctl-destroy-device.md)
