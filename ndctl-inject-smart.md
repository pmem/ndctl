---
title: ndctl
layout: pmdk
---

NAME
====

ndctl-inject-smart - perform smart threshold/injection operations on a DIMM

SYNOPSIS
========

>     ndctl inject-smart <dimm> [<options>]

DESCRIPTION
===========

A generic DIMM device object, named /dev/nmemX, is registered for each memory device indicated in the ACPI NFIT table, or other platform NVDIMM resource discovery mechanism.

ndctl-inject-smart can be used to set smart thresholds, and inject smart attributes.

EXAMPLES
========

Set smart controller temperature and spares threshold for DIMM-0 to 32C, spares threshold to 8, and enable the spares alarm.

>     ndctl inject-smart --ctrl-temperature-threshold=32 --spares-threshold=8 --spares-alarm nmem0

Inject a media temperature value of 52 and fatal health status flag for DIMM-0

>     ndctl inject-smart --media-temperature=52 --health=fatal nmem0

OPTIONS
=======

`-b; --bus=`  
Enforce that the operation only be carried on devices that are attached to the given bus. Where *bus* can be a provider name or a bus id number.

`-m; --media-temperature=`  
Inject &lt;value&gt; for the media temperature smart attribute.

`-M; --media-temperature-threshold=`  
Set &lt;value&gt; for the smart media temperature threshold.

`--media-temperature-alarm=`  
Enable or disable the smart media temperature alarm. Options are *on* or *off*.

`-c; --ctrl-temperature=`  
Inject &lt;value&gt; for the controller temperature smart attribute.

`-C; --ctrl-temperature-threshold=`  
Set &lt;value&gt; for the smart controller temperature threshold.

`--ctrl-temperature-alarm=`  
Enable or disable the smart controller temperature alarm. Options are *on* or *off*.

`-s; --spares=`  
Inject &lt;value&gt; for the spares smart attribute.

`-S; --spares-threshold=`  
Set &lt;value&gt; for the smart spares threshold.

`--spares-alarm=`  
Enable or disable the smart spares alarm. Options are *on* or *off*.

`-H; --health=`  
Smart attribute for health status. Provide either *fatal* or *nominal* to set the state of the attribute.

`-U; --unsafe-shutdown=`  
Set the flag to spoof an unsafe shutdown on the next power down.

`-v; --verbose`  
Emit debug messages for the error injection process

`-u; --human`  
Format numbers representing storage sizes, or offsets as human readable strings with units instead of the default machine-friendly raw-integer data. Convert other numeric fields into hexadecimal strings.

COPYRIGHT
=========

Copyright (c) 2018, Intel Corporation. License GPLv2: GNU GPL version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you are free to change and redistribute it. There is NO WARRANTY, to the extent permitted by law.

SEE ALSO
========

[ndctl-list](ndctl-list.md),
