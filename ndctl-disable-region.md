---
title: ndctl
layout: pmdk
---

NAME
====

ndctl-disable-region - disable the given region(s) and all descendant
namespaces

SYNOPSIS
========

>     ndctl disable-region <region> [<options>]

DESCRIPTION
===========

A generic REGION device is registered for each PMEM range or
BLK-aperture set. LIBNVDIMM provides a built-in driver for these REGION
devices. This driver is responsible for reconciling the aliased DPA
mappings across all regions, parsing the LABEL, if present, and then
emitting NAMESPACE devices with the resolved/exclusive DPA-boundaries
for the nd\_pmem or nd\_blk device driver to consume.

OPTIONS
=======

\<region\>  
A *regionX* device name, or a region id number. Restrict the operation
to the specified region(s). The keyword *all* can be specified to
indicate the lack of any restriction, however this is the same as not
supplying a --region option at all.

`-b; --bus=`  
A bus id number, or a provider string (e.g. "ACPI.NFIT"). Restrict the
operation to the specified bus(es). The keyword *all* can be specified
to indicate the lack of any restriction, however this is the same as not
supplying a --bus option at all.

COPYRIGHT
=========

Copyright © 2016 - 2020, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

SEE ALSO
========

[ndctl-enable-region](ndctl-enable-region.md)
