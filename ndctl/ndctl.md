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

[ndctl-create-namespace](ndctl-create-namespace) , [ndctl-destroy-namespace](ndctl-destroy-namespace) ,
[ndctl-check-namespace](ndctl-check-namespace) , [ndctl-enable-region](ndctl-enable-region) , [ndctl-disable-region](ndctl-disable-region) ,
[ndctl-enable-dimm](ndctl-enable-dimm) , [ndctl-disable-dimm](ndctl-disable-dimm) , [ndctl-enable-namespace](ndctl-enable-namespace) ,
[ndctl-disable-namespace](ndctl-disable-namespace) , [ndctl-zero-labels](ndctl-zero-labels) , [ndctl-read-labels](ndctl-read-labels) ,
[ndctl-inject-error](ndctl-inject-error) , [ndctl-list](ndctl-list) , [LIBNVDIMM
Overview](https://www.kernel.org/doc/Documentation/nvdimm/nvdimm.txt),
[NVDIMM Driver Writer’s
Guide](http://pmem.io/documents/NVDIMM_Driver_Writers_Guide.pdf)
