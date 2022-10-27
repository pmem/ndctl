---
layout: page
---

# NAME

cxl-disable-bus - Shutdown an entire tree of CXL devices

# SYNOPSIS

>     cxl disable-bus <root0> [<root1>..<rootN>] [<options>]

For test and debug scenarios, disable a CXL bus and any associated
memory devices from CXL.mem operations.

# OPTIONS

`-f; --force`  
DANGEROUS: Override the safety measure that blocks attempts to disable a
bus if the tool determines a descendent memdev is in active usage.
Recall that CXL memory ranges might have been established by platform
firmware and disabling an active device is akin to force removing memory
from a running system.

`--debug`  
If the cxl tool was built with debug disabled, turn on debug messages.

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl-disable-port](cxl-disable-port)
