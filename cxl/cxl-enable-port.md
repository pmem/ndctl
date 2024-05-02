---
layout: page
---

# NAME

cxl-enable-port - activate / hot-add a given CXL port

# SYNOPSIS

>     cxl enable-port <port0> [<port1>..<portN>] [<options>]

A port typically autoenables at initial device discovery. However, if it
was manually disabled this command can trigger the kernel to activate it
again. This involves detecting the state of the HDM (Host Managed Device
Memory) Decoders and validating that CXL.mem is enabled for each port in
the device’s hierarchy.

Given any enable or disable command, if the operation is a no-op due to
the current state of a target (i.e. already enabled or disabled), it is
still considered successful when executed even if no actual operation is
performed. The target can be a bus, decoder, memdev, or region. The
operation will still succeed, and report the number of
bus/decoder/memdev/region operated on, even if the operation is a no-op.

# OPTIONS

`-e; --endpoint`  
Toggle from treating the port arguments as Switch Port identifiers to
Endpoint Port identifiers.

`-m; --enable-memdevs`  
Try to enable descendant memdevs after enabling the port. Recall that a
memdev is only enabled after all CXL ports in its device topology
ancestry are enabled.

<!-- -->

`--debug`  
Turn on additional debug messages including library debug.

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl-disable-port](cxl-disable-port)
