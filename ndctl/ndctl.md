---
layout: page
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

`ndctl-create-namespace(1)` , `ndctl-destroy-namespace(1)` ,
`ndctl-check-namespace(1)` , `ndctl-enable-region(1)` ,
`ndctl-disable-region(1)` , `ndctl-enable-dimm(1)` ,
`ndctl-disable-dimm(1)` , `ndctl-enable-namespace(1)` ,
`ndctl-disable-namespace(1)` , `ndctl-zero-labels(1)` ,
`ndctl-read-labels(1)` , `ndctl-inject-error(1)` , `ndctl-list(1)` ,
[LIBNVDIMM
Overview](https://www.kernel.org/doc/Documentation/nvdimm/nvdimm.txt),
[NVDIMM Driver Writer’s
Guide](http://pmem.io/documents/NVDIMM_Driver_Writers_Guide.pdf)
