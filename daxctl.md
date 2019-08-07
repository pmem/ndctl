---
title: ndctl
layout: pmdk
---

NAME
====

daxctl - Provides enumeration and provisioning commands for the Linux
kernel Device-DAX facility

SYNOPSIS
========

>     daxctl [--version] [--help] COMMAND [ARGS]

OPTIONS
=======

`-v; --version`  
Display daxctl version.

`-h; --help`  
Run daxctl help command.

DESCRIPTION
===========

The daxctl utility provides enumeration and provisioning commands for
the Linux kernel Device-DAX facility. This facility enables DAX mappings
of performance / feature differentiated memory without need of a
filesystem.

COPYRIGHT
=========

Copyright (c) 2016 - 2019, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

SEE ALSO
========

[ndctl-create-namespace](ndctl-create-namespace.md), [ndctl-list](ndctl-list.md)
