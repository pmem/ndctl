---
layout: page
---

# NAME

cxl-wait-sanitize - wait for a sanitize operation to complete

# SYNOPSIS

>     cxl wait-sanitize <mem0> [<mem1>..<memN>] [<options>]

# DESCRIPTION

A sanitize operation can take several seconds to complete. Block and
wait for the sanitize operation to complete.

# EXAMPLE

    # cxl wait-sanitize mem0
    sanitize completed on 1 mem device

# OPTIONS

`-b; --bus=`  
Restrict the operation to the specified bus.

`-t; --timeout`  
Milliseconds to wait before timing out and returning. Defaults to
infinite.

<!-- -->

`-v; --verbose`  
Emit more debug messages

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl-list](cxl-list),
