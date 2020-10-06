---
title: ndctl
layout: pmdk
---

NAME
====

daxctl-reconfigure-device - Reconfigure a dax device into a different
mode

SYNOPSIS
========

>     daxctl reconfigure-device <dax0.0> [<dax1.0>…​<daxY.Z>] [<options>]

EXAMPLES
========

-   Reconfigure dax0.0 to system-ram mode, don’t online the memory

<!-- -->

    # daxctl reconfigure-device --mode=system-ram --no-online dax0.0
    [
      {
        "chardev":"dax0.0",
        "size":16777216000,
        "target_node":2,
        "mode":"system-ram"
      }
    ]

-   Reconfigure dax0.0 to devdax mode, attempt to offline the memory

<!-- -->

    # daxctl reconfigure-device --human --mode=devdax --force dax0.0
    {
      "chardev":"dax0.0",
      "size":"15.63 GiB (16.78 GB)",
      "target_node":2,
      "mode":"devdax"
    }

-   Reconfigure all dax devices on region0 to system-ram mode

<!-- -->

    # daxctl reconfigure-device --mode=system-ram --region=0 all
    [
      {
        "chardev":"dax0.0",
        "size":16777216000,
        "target_node":2,
        "mode":"system-ram"
      },
      {
        "chardev":"dax0.1",
        "size":16777216000,
        "target_node":3,
        "mode":"system-ram"
      }
    ]

-   Run a process called *some-service* using numactl to restrict its
    cpu nodes to *0* and *1*, and memory allocations to node 2
    (determined using daxctl\_dev\_get\_target\_node() or *daxctl list*)

<!-- -->

    # daxctl reconfigure-device --mode=system-ram dax0.0
    [
      {
        "chardev":"dax0.0",
        "size":16777216000,
        "target_node":2,
        "mode":"system-ram"
      }
    ]

    # numactl --cpunodebind=0-1 --membind=2 -- some-service --opt1 --opt2

DESCRIPTION
===========

Reconfigure the operational mode of a dax device. This can be used to
convert a regular *devdax* mode device to the *system-ram* mode which
arranges for the dax range to be hot-plugged into the system as regular
memory.

> **Note**
>
> This is a destructive operation. Any data on the dax device **will**
> be lost.

> **Note**
>
> Device reconfiguration depends on the dax-bus device model. See
> [daxctl-migrate-device-model](daxctl-migrate-device-model.md) for more information. If
> dax-class is in use (via the dax\_pmem\_compat driver), the
> reconfiguration will fail with an error such as the following:

    # daxctl reconfigure-device --mode=system-ram --region=0 all
    libdaxctl: daxctl_dev_disable: dax3.0: error: device model is dax-class
    dax3.0: disable failed: Operation not supported
    error reconfiguring devices: Operation not supported
    reconfigured 0 devices

*daxctl-reconfigure-device* nominally expects that it will online new
memory blocks as *movable*, so that kernel data doesn’t make it into
this memory. However, there are other potential agents that may be
configured to automatically online new hot-plugged memory as it appears.
Most notably, these are the
*/sys/devices/system/memory/auto\_online\_blocks* configuration, or
system udev rules. If such an agent races to online memory sections,
daxctl checks if the blocks were onlined as *movable* memory. If this
was not the case, and the memory blocks are found to be in a different
zone, then a warning is displayed. If it is desired that a different
agent control the onlining of memory blocks, and the associated memory
zone, then it is recommended to use the --no-online option described
below. This will abridge the device reconfiguration operation to just
hotplugging the memory, and refrain from then onlining it.

OPTIONS
=======

`-r; --region=`  
Restrict the operation to devices belonging to the specified region(s).
A device-dax region is a contiguous range of memory that hosts one or
more /dev/daxX.Y devices, where X is the region id and Y is the device
instance id.

`-m; --mode=`  
Specify the mode to which the dax device(s) should be reconfigured.

-   "system-ram": hotplug the device into system memory.

-   "devdax": switch to the normal "device dax" mode. This requires the
    kernel to support hot-unplugging *kmem* based memory. If this is not
    available, a reboot is the only way to switch back to *devdax* mode.

`-N; --no-online`  
By default, memory sections provided by system-ram devices will be
brought online automatically and immediately with the *online\_movable*
policy. Use this option to disable the automatic onlining behavior.

<!-- -->

`--no-movable`  
*--movable* is the default. This can be overridden to online new memory
such that it is not *movable*. This allows any allocation to potentially
be served from this memory. This may preclude subsequent removal. With
the *--movable* behavior (which is default), kernel allocations will not
consider this memory, and it will be reserved for application use.

`-f; --force`  
When converting from "system-ram" mode to "devdax", it is expected that
all the memory sections are first made offline. By default, daxctl won’t
touch online memory. However with this option, attempt to offline the
memory on the NUMA node associated with the dax device before converting
it back to "devdax" mode.

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

[daxctl-list](daxctl-list.md),[daxctl-migrate-device-model](daxctl-migrate-device-model.md)
