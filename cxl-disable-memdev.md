---
title: ndctl
layout: pmdk
---

# NAME

cxl-disable-memdev - deactivate / hot-remove a given CXL memdev

# SYNOPSIS

>     cxl disable-memdev <mem0> [<mem1>..<memN>] [<options>]

# OPTIONS

\<memory device(s)>  
A *memX* device name, or a memdev id number. Restrict the operation to
the specified memdev(s). The keyword *all* can be specified to indicate
the lack of any restriction.

`-S; --serial`  
Rather an a memdev id number, interpret the \<memdev> argument(s) as a
list of serial numbers.

`-f; --force`  
DANGEROUS: Override the safety measure that blocks attempts to disable a
device if the tool determines the memdev is in active usage. Recall that
CXL memory ranges might have been established by platform firmware and
disabling an active device is akin to force removing memory from a
running system.

`-v`  
Turn on verbose debug messages in the library (if libcxl was built with
logging and debug enabled).

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl-enable-memdev](cxl-enable-memdev.md)
