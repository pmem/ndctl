---
layout: page
---

# NAME

cxl-disable-memdev - deactivate / hot-remove a given CXL memdev

# SYNOPSIS

>     cxl disable-memdev <mem0> [<mem1>..<memN>] [<options>]

Given any enable or disable command, if the operation is a no-op due to
the current state of a target (i.e. already enabled or disabled), it is
still considered successful when executed even if no actual operation is
performed. The target can be a bus, decoder, memdev, or region. The
operation will still succeed, and report the number of
bus/decoder/memdev/region operated on, even if the operation is a no-op.

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

`-f; --force`  
DANGEROUS: Override the safety measure that blocks attempts to disable a
device if the tool determines the memdev is in active usage. Recall that
CXL memory ranges might have been established by platform firmware and
disabling an active device is akin to force removing memory from a
running system. Going down this path does not offline active memory if
they are currently online. User is recommended to offline and disable
the appropriate regions before disabling the memdevs.

`-v`  
Turn on verbose debug messages in the library (if libcxl was built with
logging and debug enabled).

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl-enable-memdev](cxl-enable-memdev)
