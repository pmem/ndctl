---
title: ndctl
layout: pmdk
---

# NAME

cxl-list - List platform CXL objects, and their attributes, in json.

# SYNOPSIS

>     cxl list [<options>]

Walk the CXL capable device hierarchy in the system and list all device
instances along with some of their major attributes.

Options can be specified to limit the output to specific objects. When a
single object type is specified the return json object is an array of
just those objects, when multiple objects types are specified the
returned the returned object may be an array of arrays with the inner
array named for the given object type. The top-level arrays are ellided
when the objects can nest under a higher object-type in the hierararchy.
The potential top-level array names and their nesting properties are:

"anon memdevs"  
(disabled memory devices) do not nest

"buses"  
do not nest

"ports"  
nest under buses

"endpoints"  
nest under ports or buses (if ports are not emitted)

"memdevs"  
nest under endpoints or ports (if endpoints are not emitted) or buses
(if endpoints and ports are not emitted)

"root decoders"  
nest under buses

"port decoders"  
nest under ports, or buses (if ports are not emitted)

"endpoint decoders"  
nest under endpoints, or ports (if endpoints are not emitted) or buses
(if endpoints and ports are not emitted)

Filters can by specifed as either a single identidier, a space separated
quoted string, or a comma separated list. When multiple filter
identifiers are specified within a filter string, like "-m
mem0,mem1,mem2", they are combined as an *OR* filter. When multiple
filter string types are specified, like "-m mem0,mem1,mem2 -p port10",
they are combined as an *AND* filter. So, "-m mem0,mem1,mem2 -p port10"
would only list objects that are beneath port10 AND map mem0, mem1, OR
mem2.

Given that many topology queries seek to answer questions relative to a
given memdev, buses, ports, endpoints, and decoders can be filtered by
one or more memdevs. For example:

    # cxl list -P -p switch,endpoint -m mem0
    [
      {
        "port":"port1",
        "host":"ACPI0016:00",
        "endpoints:port1":[
          {
            "endpoint":"endpoint2",
            "host":"mem0"
          }
        ]
      }
    ]

Additionally, when provisioning new interleave configurations it is
useful to know which memdevs can be referenced by a given decoder like a
root decoder, or mapped by a given port if the decoders are not
configured.

    # cxl list -Mu -d decoder0.0
    {
      "memdev":"mem0",
      "pmem_size":"256.00 MiB (268.44 MB)",
      "ram_size":0,
      "serial":"0",
      "host":"0000:35:00.0"
    }

The --human option in addition to reformatting some fields to more human
friendly strings also unwraps the array to reduce the number of lines of
output.

# EXAMPLE

    # cxl list --memdevs
    [
      {
        "memdev":"mem0",
        "pmem_size":268435456,
        "ram_size":0,
        "serial":0,
        "host":"0000:35:00.0"
      }
    ]

    # cxl list -BMu
    [
      {
        "anon memdevs":[
          {
            "memdev":"mem0",
            "pmem_size":"256.00 MiB (268.44 MB)",
            "ram_size":0,
            "serial":"0"
          }
        ]
      },
      {
        "buses":[
          {
            "bus":"root0",
            "provider":"ACPI.CXL"
          }
        ]
      }
    ]

# OPTIONS

`-m; --memdev=`  
Specify CXL memory device name(s), or device id(s), to filter the
listing. For example:

<!-- -->

    # cxl list -M --memdev="0 mem3 5"
    [
      {
        "memdev":"mem0",
        "pmem_size":268435456,
        "ram_size":0,
        "serial":0
      },
      {
        "memdev":"mem3",
        "pmem_size":268435456,
        "ram_size":268435456,
        "serial":2
      },
      {
        "memdev":"mem5",
        "pmem_size":268435456,
        "ram_size":268435456,
        "serial":4
      }
    ]

`-s; --serial=`  
Specify CXL memory device serial number(s) to filter the listing

`-M; --memdevs`  
Include CXL memory devices in the listing

`-i; --idle`  
Include idle (not enabled / zero-sized) devices in the listing

`-H; --health`  
Include health information in the memdev listing. Example listing:

<!-- -->

    # cxl list -m mem0 -H
    [
      {
        "memdev":"mem0",
        "pmem_size":268435456,
        "ram_size":268435456,
        "health":{
          "maintenance_needed":true,
          "performance_degraded":true,
          "hw_replacement_needed":true,
          "media_normal":false,
          "media_not_ready":false,
          "media_persistence_lost":false,
          "media_data_lost":true,
          "media_powerloss_persistence_loss":false,
          "media_shutdown_persistence_loss":false,
          "media_persistence_loss_imminent":false,
          "media_powerloss_data_loss":false,
          "media_shutdown_data_loss":false,
          "media_data_loss_imminent":false,
          "ext_life_used":"normal",
          "ext_temperature":"critical",
          "ext_corrected_volatile":"warning",
          "ext_corrected_persistent":"normal",
          "life_used_percent":15,
          "temperature":25,
          "dirty_shutdowns":10,
          "volatile_errors":20,
          "pmem_errors":30
        }
      }
    ]

`-I; --partition`  
Include partition information in the memdev listing. Example listing:

<!-- -->

    # cxl list -m mem0 -I
    [
      {
        "memdev":"mem0",
        "pmem_size":0,
        "ram_size":273535729664,
        "partition_info":{
          "total_size":273535729664,
          "volatile_only_size":0,
          "persistent_only_size":0,
          "partition_alignment_size":268435456
          "active_volatile_size":273535729664,
          "active_persistent_size":0,
          "next_volatile_size":0,
          "next_persistent_size":0,
        }
      }
    ]

`-B; --buses`  
Include *bus* / CXL root object(s) in the listing. Typically, on ACPI
systems the bus object is a singleton associated with the ACPI0017
device, but there are test scenerios where there may be multiple CXL
memory hierarchies.

<!-- -->

    # cxl list -B
    [
      {
        "bus":"root3",
        "provider":"cxl_test"
      },
      {
        "bus":"root0",
        "provider":"ACPI.CXL"
      }
    ]

`-b; --bus=`  
Specify CXL root device name(s), device id(s), and / or CXL bus provider
names to filter the listing. The supported provider names are "ACPI.CXL"
and "cxl_test".

`-P; --ports`  
Include port objects (CXL / PCIe root ports + Upstream Switch Ports) in
the listing.

`-p; --port=`  
Specify CXL Port device name(s), device id(s), and or port type names to
filter the listing. The supported port type names are "root" and
"switch". Note that a bus object is also a port, so the following two
syntaxes are equivalent:

<!-- -->

    # cxl list -B
    # cxl list -P -p root -S

    ...where the '-S/--single' is required since descendant ports are always
    included in a port listing and '-S/--single' stops after listing the
    bus.  Additionally, endpoint objects are ports so the following commands
    are equivalent, and no '-S/--single' is required as endpoint ports are
    terminal:

    # cxl list -E
    # cxl list -P -p endpoint

    By default, only 'switch' ports are listed, i.e.

    # cxl list -P
    # cxl list -P -p switch

    ...are equivalent.

`-S; --single`  
Specify whether the listing should emit all the objects that are
descendants of a port that matches the port filter, or only direct
descendants of the individual ports that match the filter. By default
all descendant objects are listed.

`-E; --endpoints`  
Include endpoint objects (CXL Memory Device decoders) in the listing.

<!-- -->

    # cxl list -E
    [
      {
        "endpoint":"endpoint2",
        "host":"mem0"
      }
    ]

`-e; --endpoint`  
Specify CXL endpoint device name(s), or device id(s) to filter the
emitted endpoint(s).

`-D; --decoders`  
Include decoder objects (CXL Memory decode capability instances in
buses, ports, and endpoints) in the listing.

`-d; --decoder`  
Specify CXL decoder device name(s), device id(s), or decoder type names
to filter the emitted decoder(s). The format for a decoder name is
"decoder\<port_id>.\<instance_id>". The possible decoder type names are
"root", "switch", or "endpoint", similar to the port filter syntax.

`-T; --targets`  
Extend decoder listings with downstream port target information, port
and bus listings with the downstream port information, and / or regions
with mapping information.

<!-- -->

    # cxl list -BTu -b ACPI.CXL
    {
      "bus":"root0",
      "provider":"ACPI.CXL",
      "nr_dports":1,
      "dports":[
        {
          "dport":"ACPI0016:00",
          "alias":"pci0000:34",
          "id":"0"
        }
      ]
    }

`-R; --regions`  
Include region objects in the listing.

`-r; --region`  
Specify CXL region device name(s), or device id(s), to filter the
listing.

`-v; --verbose`  
Increase verbosity of the output. This can be specified multiple times
to be even more verbose on the informational and miscellaneous output,
and can be used to override omitted flags for showing specific
information. Note that cxl list --verbose --verbose is equivalent to cxl
list -vv.

-   **-v** Enable --memdevs, --regions, --buses, --ports, --decoders,
    and --targets.

-   **-vv** Everything **-v** provides, plus include disabled devices
    with --idle.

-   **-vvv** Everything **-vv** provides, plus enable --health and
    --partition.

`--debug`  
If the cxl tool was built with debug enabled, turn on debug messages.

<!-- -->

`-u; --human`  
By default the command will output machine-friendly raw-integer data.
Instead, with this flag, numbers representing storage size will be
formatted as human readable strings with units, other fields are
converted to hexadecimal strings.

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[ndctl-list](ndctl-list.md)
