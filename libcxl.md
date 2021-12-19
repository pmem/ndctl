---
title: ndctl
layout: pmdk
---

# NAME

libcxl - A library to interact with CXL devices through sysfs(5) and
ioctl(2) interfaces

# SYNOPSIS

>     #include <cxl/libcxl.h>
>     cc …​ -lcxl

# DESCRIPTION

libcxl provides interfaces to interact with CXL devices in Linux, using
sysfs interfaces for most kernel interactions, and the ioctl() interface
for command submission.

The starting point for all library interfaces is a *cxl_ctx* object,
returned by [cxl_new](cxl_new.md). CXL *Type 3* memory devices are
children of the cxl_ctx object, and can be iterated through using an
iterator API.

Library level interfaces that are agnostic to any device, or a specific
subclass of operations have the prefix *cxl\_*

The object representing a CXL Type 3 device is *cxl_memdev*. Library
interfaces related to these devices have the prefix *cxl_memdev\_*.
These interfaces are mostly associated with sysfs interactions (unless
otherwise noted in their respective documentation pages). They are
typically used to retrieve data published by the kernel, or to send data
or trigger kernel operations for a given device.

A *cxl_cmd* is a reference counted object which is used to perform
*Mailbox* commands as described in the CXL Specification. A *cxl_cmd*
object is tied to a *cxl_memdev*. Associated library interfaces have the
prefix *cxl_cmd\_*. Within this sub-class of interfaces, there are:

-   *cxl_cmd_new\_\** interfaces that allocate a new cxl_cmd object for
    a given command type.

-   *cxl_cmd_submit* which submits the command via ioctl()

-   *cxl_cmd\_\<name>\_get\_\<field>* interfaces that get specific
    fields out of the command response

-   *cxl_cmd_get\_\** interfaces to get general command related
    information.

# COPYRIGHT

Copyright © 2016 - 2020, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl](cxl.md)
