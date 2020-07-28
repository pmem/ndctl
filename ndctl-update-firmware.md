---
title: ndctl
layout: pmdk
---

NAME
====

ndctl-update-firmware - provides for updating the firmware on an NVDIMM

SYNOPSIS
========

>     ndctl update-firmware <dimm> [<options>]

DESCRIPTION
===========

Provide a generic interface for updating NVDIMM firmware. The use of
this depends on support from the underlying libndctl, kernel, as well as
the platform itself.

OPTIONS
=======

\<dimm\>  
A *nmemX* device name, or a dimm id number. Restrict the operation to
the specified dimm(s). The keyword *all* can be specified to indicate
the lack of any restriction, however this is the same as not supplying a
--dimm option at all.

`-b; --bus=`  
A bus id number, or a provider string (e.g. "ACPI.NFIT"). Restrict the
operation to the specified bus(es). The keyword *all* can be specified
to indicate the lack of any restriction, however this is the same as not
supplying a --bus option at all.

`-f; --firmware`  
firmware file used to perform the update

`-v; --verbose`  
Emit debug messages for the namespace check process.

COPYRIGHT
=========

Copyright (c) 2016 - 2019, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.
