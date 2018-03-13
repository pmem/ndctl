NAME
====

ndctl-disable-namespace - disable the given namespace(s)

SYNOPSIS
========

>     ndctl disable-namespace <namespace> [<options>]

DESCRIPTION
===========

A REGION, after resolving DPA aliasing and LABEL specified boundaries, surfaces one or more "namespace" devices. The arrival of a "namespace" device currently triggers either the nd\_blk or nd\_pmem driver to load and register a disk/block device.

OPTIONS
=======

&lt;namespace&gt;  
A *namespaceX.Y* device name. The keyword *all* can be specified to carry out the operation on every namespace in the system, optionally filtered by region (see --region=option)

-r; --region=  
A *regionX* device name, or a region id number. The keyword *all* can be specified to carry out the operation on every region in the system, optionally filtered by bus id (see --bus= option).

-b; --bus=  
Enforce that the operation only be carried on devices that are attached to the given bus. Where *bus* can be a provider name or a bus id number.

-v; --verbose  
Emit debug messages for the namespace operation

COPYRIGHT
=========

Copyright (c) 2016 - 2017, Intel Corporation. License GPLv2: GNU GPL version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you are free to change and redistribute it. There is NO WARRANTY, to the extent permitted by law.

SEE ALSO
========

[ndctl-disable-namespace](ndctl-disable-namespace.md)
