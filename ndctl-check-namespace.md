---
title: ndctl
layout: pmdk
---

NAME
====

ndctl-check-namespace - check namespace metadata consistency

SYNOPSIS
========

>     ndctl check-namespace <namespace> [<options>]

DESCRIPTION
===========

A namespace in the *sector* mode will have metadata on it to describe
the kernel BTT (Block Translation Table). The check-namespace command
can be used to check the consistency of this metadata, and optionally,
also attempt to repair it, if it has enough information to do so.

The namespace being checked has to be disabled before initiating a check
on it as a precautionary measure. The --force option can override this.

EXAMPLES
========

Check a namespace (only report errors)

>     ndctl disable-namespace namespace0.0
>     ndctl check-namespace namespace0.0

Check a namespace, and perform repairs if possible

>     ndctl disable-namespace namespace0.0
>     ndctl check-namespace --repair namespace0.0

OPTIONS
=======

`-R; --repair`  
Perform metadata repairs if possible. Without this option, the raw
namespace contents will not be touched.

`-L; --rewrite-log`  
Regenerate the BTT log and write it to media. This can be used to
convert from the old (pre 4.15) padding format that was incompatible
with other BTT implementations to the updated format. This requires the
--repair option to be provided.

    WARNING: Do not interrupt this operation as it can potentially cause
    unrecoverable metadata corruption. It is highly recommended to create
    a backup of the raw namespace before attempting this.

`-f; --force`  
Unless this option is specified, a check-namespace operation will fail
if the namespace is presently active. Specifying --force causes the
namespace to be disabled before checking.

`-v; --verbose`  
Emit debug messages for the namespace check process.

`-r; --region=`  
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

Copyright (c) 2016 - 2020, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

SEE ALSO
========

[ndctl-disable-namespace](ndctl-disable-namespace.md) , [ndctl-enable-namespace](ndctl-enable-namespace.md) , [UEFI NVDIMM Label
Protocol](http://www.uefi.org/sites/default/files/resources/UEFI_Spec_2_7.pdf)
