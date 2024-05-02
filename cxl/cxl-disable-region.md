---
layout: page
---

# NAME

cxl-disable-region - disable specified region(s).

# SYNOPSIS

>     cxl disable-region <region> [<options>]

# DESCRIPTION

A CXL region is composed of one or more slices of CXL memdevs, with
configurable interleave settings - both the number of interleave ways,
and the interleave granularity.

If there are memory blocks that are still online, the operation will
attempt to offline the relevant blocks. If the offlining fails, the
operation fails when not using the -f (force) parameter.

# EXAMPLE

    # cxl disable-region all
    disabled 2 regions

Given any enable or disable command, if the operation is a no-op due to
the current state of a target (i.e. already enabled or disabled), it is
still considered successful when executed even if no actual operation is
performed. The target can be a bus, decoder, memdev, or region. The
operation will still succeed, and report the number of
bus/decoder/memdev/region operated on, even if the operation is a no-op.

# OPTIONS

`-b; --bus=`  
Restrict the operation to the specified bus.

`-f; --force`  
Attempt to disable-region even though memory cannot be offlined
successfully. Will emit warning that operation will permanently leak
physical address space and cannot be recovered until a reboot.

<!-- -->

`-d; --decoder=`  
The root decoder to limit the operation to. Only regions that are
children of the specified decoder will be acted upon.

<!-- -->

`--debug`  
Turn on additional debug messages including library debug.

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl-list](cxl-list), [cxl-enable-region](cxl-enable-region)
