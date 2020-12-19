---
title: ndctl
layout: pmdk
---

NAME
====

daxctl-destroy-device - Destroy a devdax device

SYNOPSIS
========

>     daxctl destroy-device <dax0.0> [<dax1.0>…​<daxY.Z>] [<options>]

EXAMPLES
========

-   Destroys dax0.1

<!-- -->

    # daxctl disable-device dax0.1
    disabled 1 device
    # daxctl destroy-device dax0.1
    destroyed 1 device

-   Destroys all devices in region id 0

<!-- -->

    # daxctl disable-device -r 0 all
    disabled 3 devices
    # daxctl destroy-device -r 0 all
    destroyed 2 devices

DESCRIPTION
===========

Destroys a dax device in *devdax* mode.

OPTIONS
=======

`-r; --region=`  
Restrict the operation to devices belonging to the specified region(s).
A device-dax region is a contiguous range of memory that hosts one or
more /dev/daxX.Y devices, where X is the region id and Y is the device
instance id.

<!-- -->

`-u; --human`  
By default the command will output machine-friendly raw-integer data.
Instead, with this flag, numbers representing storage size will be
formatted as human readable strings with units, other fields are
converted to hexadecimal strings.

<!-- -->

`-v; --verbose`  
Emit more debug messages

COPYRIGHT
=========

Copyright © 2016 - 2020, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

SEE ALSO
========

[daxctl-list](daxctl-list.md),[daxctl-reconfigure-device](daxctl-reconfigure-device.md),[daxctl-create-device](daxctl-create-device.md)
