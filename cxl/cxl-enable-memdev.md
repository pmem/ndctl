---
layout: page
---

# NAME

cxl-enable-memdev - activate / hot-add a given CXL memdev

# SYNOPSIS

>     cxl enable-memdev <mem0> [<mem1>..<memN>] [<options>]

A memdev typically autoenables at initial device discovery. However, if
it was manually disabled this command can trigger the kernel to activate
it again. This involves detecting the state of the HDM (Host Managed
Device Memory) Decoders and validating that CXL.mem is enabled for each
port in the device’s hierarchy.

# OPTIONS

\<memory device(s)\>  
A *memX* device name, or a memdev id number. Restrict the operation to
the specified memdev(s). The keyword *all* can be specified to indicate
the lack of any restriction.

`-S; --serial`  
Rather an a memdev id number, interpret the \<memdev\> argument(s) as a
list of serial numbers.

`-v`  
Turn on verbose debug messages in the library (if libcxl was built with
logging and debug enabled).

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl-disable-memdev](cxl-disable-memdev)
