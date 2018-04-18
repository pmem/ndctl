---
title: ndctl
layout: pmdk
---

NAME
====

ndctl-disable-dimm - disable one or more idle dimms

SYNOPSIS
========

>     ndctl disable-dimm <dimm> [<options>]

DESCRIPTION
===========

A generic DIMM device object, named /dev/nmemX, is registered for each memory device indicated in the ACPI NFIT table, or other platform NVDIMM resource discovery mechanism. The LIBNVDIMM core provides a built-in driver for these DIMM devices. The driver is responsible for determining if the DIMM implements a namespace label area, and initializing the kernel’s in-memory copy of that label data.

The kernel performs runtime modifications of that data when namespace provisioning actions are taken, and actively blocks userspace from initiating label data changes while the DIMM is active in any region. Disabling a DIMM, after all the regions it is a member of have been disabled, allows userspace to manually update the label data to be consumed when the DIMM is next enabled.

OPTIONS
=======

`<dimm>`  
    A 'nmemX' device name, or a dimm id number. The keyword 'all' can
    be specified to carry out the operation on every dimm in the system,
    optionally filtered by bus id (see --bus= option).

`-b; --bus=`  
Enforce that the operation only be carried on devices that are attached to the given bus. Where *bus* can be a provider name or a bus id number.

COPYRIGHT
=========

Copyright (c) 2016 - 2018, Intel Corporation. License GPLv2: GNU GPL version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you are free to change and redistribute it. There is NO WARRANTY, to the extent permitted by law.

SEE ALSO
========

[ndctl-enable-dimm](ndctl-enable-dimm.md)
