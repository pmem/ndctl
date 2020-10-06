---
title: ndctl
layout: pmdk
---

NAME
====

daxctl-online-memory - Online the memory for a device that is in
system-ram mode

SYNOPSIS
========

>     daxctl online-memory <dax0.0> [<dax1.0>…​<daxY.Z>] [<options>]

EXAMPLES
========

-   Reconfigure dax0.0 to system-ram mode, don’t online the memory

<!-- -->

    # daxctl reconfigure-device --mode=system-ram --no-online --human dax0.0
    {
      "chardev":"dax0.0",
      "size":"7.87 GiB (8.45 GB)",
      "target_node":2,
      "mode":"system-ram"
    }

-   Online the memory separately

<!-- -->

    # daxctl online-memory dax0.0
    dax0.0: 62 new sections onlined
    onlined memory for 1 device

-   Onlining memory when some sections were already online

<!-- -->

    # daxctl online-memory dax0.0
    dax0.0: 1 section already online
    dax0.0: 61 new sections onlined
    onlined memory for 1 device

DESCRIPTION
===========

Online the memory sections associated with a device that has been
converted to the system-ram mode. If one or more blocks are already
online, print a message about them, and attempt to online the remaining
blocks.

This is complementary to the *daxctl-reconfigure-device* command, when
used with the *--no-online* option to skip onlining memory sections
immediately after the reconfigure. In these scenarios, the memory can be
onlined at a later time using *daxctl-online-memory*.

OPTIONS
=======

`-r; --region=`  
Restrict the operation to devices belonging to the specified region(s).
A device-dax region is a contiguous range of memory that hosts one or
more /dev/daxX.Y devices, where X is the region id and Y is the device
instance id.

<!-- -->

`--no-movable`  
*--movable* is the default. This can be overridden to online new memory
such that it is not *movable*. This allows any allocation to potentially
be served from this memory. This may preclude subsequent removal. With
the *--movable* behavior (which is default), kernel allocations will not
consider this memory, and it will be reserved for application use.

`-u; --human`  
By default the command will output machine-friendly raw-integer data.
Instead, with this flag, numbers representing storage size will be
formatted as human readable strings with units, other fields are
converted to hexadecimal strings.

`-v; --verbose`  
Emit more debug messages

COPYRIGHT
=========

Copyright (c) 2016 - 2020, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

SEE ALSO
========

[daxctl-reconfigure-device](daxctl-reconfigure-device.md),[daxctl-offline-memory](daxctl-offline-memory.md)
