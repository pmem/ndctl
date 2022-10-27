---
layout: page
---

# NAME

cxl-enable-region - enable specified region(s).

# SYNOPSIS

>     cxl enable-region <region> [<options>]

# DESCRIPTION

A CXL region is composed of one or more slices of CXL memdevs, with
configurable interleave settings - both the number of interleave ways,
and the interleave granularity.

# EXAMPLE

    # cxl enable-region all
    enabled 2 regions

# OPTIONS

`-b; --bus=`  
Restrict the operation to the specified bus.

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

[cxl-list](cxl-list), [cxl-disable-region](cxl-disable-region)
