---
layout: page
---

# NAME

ndctl-wait-scrub - wait for an Address Range Scrub (ARS) operation to
complete

# SYNOPSIS

>     ndctl wait-scrub [<bus-id> <bus-id2> …​ <bus-idN>] [<options>]

# DESCRIPTION

NVDIMM Address Range Scrub is a capability provided by platform firmware
that allows for the discovery of memory errors by system software. It
enables system software to pre-emptively avoid accesses that could lead
to uncorrectable memory error handling events, and it otherwise allows
memory errors to be enumerated.

The kernel provides a POLL(2) capable sysfs file (*scrub*) to indicate
the state of ARS. The *scrub* file maintains a running count of ARS runs
that have taken place. While a current run is in progress a *+*
character is emitted along with the current count. The *ndctl
wait-scrub* operation waits for *scrub*, across all specified buses, to
indicate not in-progress at least once.

# EXAMPLE

Wait for scrub on all nvdimm buses in the system. The json listing
report at the end only includes the buses that support ARS operations.

    # ndctl wait-scrub
    [
      {
        "provider":"nfit_test.1",
        "dev":"ndbus3",
        "scrub_state":"idle"
      },
      {
        "provider":"nfit_test.0",
        "dev":"ndbus2",
        "scrub_state":"idle"
      }
    ]

When specifying an individual bus, or if there is only one bus in the
system, the command reports whether ARS support is available.

    # ndctl wait-scrub e820
    error waiting for scrub completion: Operation not supported

# OPTIONS

`-v; --verbose`  
Emit debug messages for the ARS wait process

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[ndctl-start-scrub](ndctl-start-scrub) , [ACPI 6.2 Specification Section 9.20.7.2 Address
Range Scrubbing (ARS)
Overview](http://www.uefi.org/sites/default/files/resources/ACPI%206_2_A_Sept29.pdf)
