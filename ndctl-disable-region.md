---
title: ndctl
layout: pmdk
---

NAME
====

ndctl-disable-region - disable the given region(s) and all descendant namespaces

SYNOPSIS
========

>     ndctl disable-region <region> [<options>]

DESCRIPTION
===========

A generic REGION device is registered for each PMEM range or BLK-aperture set. LIBNVDIMM provides a built-in driver for these REGION devices. This driver is responsible for reconciling the aliased DPA mappings across all regions, parsing the LABEL, if present, and then emitting NAMESPACE devices with the resolved/exclusive DPA-boundaries for the nd\_pmem or nd\_blk device driver to consume.

OPTIONS
=======

&lt;region&gt;  
A *regionX* device name, or a region id number. The keyword *all* can be specified to carry out the operation on every region in the system, optionally filtered by bus id (see --bus= option).

-b; --bus=  
Enforce that the operation only be carried on devices that are attached to the given bus. Where *bus* can be a provider name or a bus id number.

COPYRIGHT
=========

Copyright (c) 2016 - 2017, Intel Corporation. License GPLv2: GNU GPL version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you are free to change and redistribute it. There is NO WARRANTY, to the extent permitted by law.

SEE ALSO
========

[ndctl-enable-region](ndctl-enable-region.md)
