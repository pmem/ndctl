---
title: ndctl
layout: pmdk
---

NAME
====

ndctl-clear-errors - clear all errors (badblocks) on the given namespace

SYNOPSIS
========

>     ndctl clear-errors <namespace> [<options>]

DESCRIPTION
===========

A namespace may have one or more *media errors*, either known to the
kernel or in a latent state. These error locations, or *badblocks* can
cause poison consumption events if read in an unsafe manner.

Moreover, these badblocks also indicate that due to media corruption,
any data that may have been in these locations has been unrecoverably
lost.

Normally, in the presence of such errors, the administrator is expected
to recover the data from out of band means (such as backups), destroy
the namespace, recreate it, and then restore the data. When the data is
re-written, the writes will allow any errors to be cleared as they are
encountered. In such a workflow, one should **never** need to use the
*clear-errors* command.

However, there may be special use cases, where the data currently on the
namespace does not matter - for example, if a *devdax* mode namespace is
being prepared for use as *system-ram*. In such cases, it may be
desirable to clear any errors on the namespace prior to switching its
mode to prevent disruptive machine checks due to poison consumption.

> **Note**
>
> **Only** use this command when the data on the namespace is
> immaterial. For any blocks that are cleared via this command, any data
> on the blocks in question will be lost, and replaced with content that
> is implementation (platform) defined, and unpredictable.

> **Warning**
>
> This is a DANGEROUS command, and should only be used after fully
> understanding its implications and consequences. This WILL erase your
> data.

For namespaces in one of *fsdax* or *davdax* modes, this command will
only consider the *data* area for error clearing. Namespace metadata,
such as info-blocks, will not be touched. For namespaces in *raw* mode,
the full available capacity of the namespace is considered for error
clearing. Namespaces that are in *sector* mode are not supported, and
will be skipped.

> **Note**
>
> It is expected that the command is run with the namespace *enabled*. A
> namespace in the *disabled* state will appear as, and will be treated
> as a *raw* namespace, and error clearing will be performed for the
> full available capacity of the namespace, including any potential
> metadata areas. If there happen to be errors in the metadata area,
> clearing them may result in unpredictable outcomes. You have been
> warned!

Known errors are ones that the kernel has encountered before, either via
a previous scrub, or by an attempted read from those locations. These
can be listed by running *ndctl list --media-errors* for a given
namespace. Latent errors, as the name indicates, are unknown to the
kernel. These can be found by running a scrub operation on the NVDIMMs
in question. By default, the ndctl-clear-errors command only clears
known errors. This can be overridden using the *--scrub* option to clear
**all** errors.

> **Note**
>
> If a scrub is in progress when the command is called, it will
> unconditionally wait for it to complete.

EXAMPLES
========

Clear errors on namespace 0.0

>         ndctl clear-errors namespace0.0

Clear errors on all namespaces belonging to region1, including scrubbing
for latent errors

>         ndctl clear-errors --scrub --region=region1 all

OPTIONS
=======

`-s; --scrub`  
Perform a *scrub* on the bus prior to clearing errors. This allows for
the clearing of any latent media errors in addition to errors the kernel
already knows about.

> **Note**
>
> This will cause the command to start and wait for a full scrub, and
> this can potentially be a very long-running operation.

`-v; --verbose`  
Emit debug messages.

`-r; --region=`  
    A 'regionX' device name, or a region id number. The keyword 'all' can
    be specified to carry out the operation on every region in the system,
    optionally filtered by bus id (see --bus= option).

`-b; --bus=`  
Enforce that the operation only be carried on devices that are attached
to the given bus. Where *bus* can be a provider name or a bus id number.

COPYRIGHT
=========

Copyright (c) 2016 - 2019, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

SEE ALSO
========

[ndctl-start-scrub](ndctl-start-scrub.md) , [ndctl-list](ndctl-list.md)
