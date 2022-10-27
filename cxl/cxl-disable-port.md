---
layout: page
---

# NAME

cxl-disable-port - disable / hot-remove a given CXL port and descendants

# SYNOPSIS

>     cxl disable-port <port0> [<port1>..<portN>] [<options>]

For test and debug scenarios, disable a CXL port and any memory devices
dependent on this port being active for CXL.mem operation.

# OPTIONS

`-e; --endpoint`  
Toggle from treating the port arguments as Switch Port identifiers to
Endpoint Port identifiers.

`-f; --force`  
DANGEROUS: Override the safety measure that blocks attempts to disable a
port if the tool determines a descendent memdev is in active usage.
Recall that CXL memory ranges might have been established by platform
firmware and disabling an active device is akin to force removing memory
from a running system.

<!-- -->

`--debug`  
Turn on additional debug messages including library debug.

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl-disable-port](cxl-disable-port)
