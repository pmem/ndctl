---
title: ndctl
layout: pmdk
---

# NAME

cxl_new - Create a new library context object that acts as a handle for
all library operations

# SYNOPSIS

    #include <cxl/libcxl.h>

    int cxl_new(struct cxl_ctx **ctx);

# DESCRIPTION

Instantiates a new library context, and stores an opaque pointer in ctx.
The context is freed by [cxl_unref](cxl_unref.md), i.e. cxl_new(3)
implies an internal [cxl_ref](cxl_ref.md).

# RETURN VALUE

Returns 0 on success, and a negative errno on failure. Possible error
codes are:

-   -ENOMEM

-   -ENXIO

# EXAMPLE

See example usage in test/libcxl.c

# COPYRIGHT

Copyright © 2016 - 2020, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl_ref](cxl_ref.md), [cxl_unref](cxl_unref.md)
