---
title: ndctl
layout: pmdk
---

# NAME

ndctl - Manage "libnvdimm" subsystem devices (Non-volatile Memory)

# SYNOPSIS

>     ndctl [--version] [--help] [OPTIONS] COMMAND [ARGS]

# OPTIONS

`-v; --version`  
Display ndctl version.

`-h; --help`  
Run ndctl help command.

# DESCRIPTION

ndctl is utility for managing the "libnvdimm" kernel subsystem. The
"libnvdimm" subsystem defines a kernel device model and control message
interface for platform NVDIMM resources like those defined by the ACPI
6.0 NFIT (NVDIMM Firmware Interface Table). Operations supported by the
tool include, provisioning capacity (namespaces), as well as
enumerating/enabling/disabling the devices (dimms, regions, namespaces)
associated with an NVDIMM bus.

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[ndctl-create-namespace](ndctl-create-namespace.md) , [ndctl-destroy-namespace](ndctl-destroy-namespace.md) ,
[ndctl-check-namespace](ndctl-check-namespace.md) , [ndctl-enable-region](ndctl-enable-region.md) , [ndctl-disable-region](ndctl-disable-region.md) ,
[ndctl-enable-dimm](ndctl-enable-dimm.md) , [ndctl-disable-dimm](ndctl-disable-dimm.md) , [ndctl-enable-namespace](ndctl-enable-namespace.md) ,
[ndctl-disable-namespace](ndctl-disable-namespace.md) , [ndctl-zero-labels](ndctl-zero-labels.md) , [ndctl-read-labels](ndctl-read-labels.md) ,
[ndctl-inject-error](ndctl-inject-error.md) , [ndctl-list](ndctl-list.md) , [LIBNVDIMM
Overview](https://www.kernel.org/doc/Documentation/nvdimm/nvdimm.txt),
[NVDIMM Driver Writer’s
Guide](http://pmem.io/documents/NVDIMM_Driver_Writers_Guide.pdf)
