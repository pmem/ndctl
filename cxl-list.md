---
title: ndctl
layout: pmdk
---

# NAME

cxl-list - List CXL capable memory devices, and their attributes in
json.

# SYNOPSIS

>     cxl list [<options>]

Walk the CXL capable device hierarchy in the system and list all device
instances along with some of their major attributes.

# EXAMPLE

    # cxl list --memdevs
    {
      "memdev":"mem0",
      "pmem_size":268435456,
      "ram_size":0,
    }

# OPTIONS

`-m; --memdev=`  
Specify a cxl memory device name to filter the listing. For example:

<!-- -->

    # cxl list --memdev=mem0
    {
      "memdev":"mem0",
      "pmem_size":268435456,
      "ram_size":0,
    }

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

`-u; --human`  
By default the command will output machine-friendly raw-integer data.
Instead, with this flag, numbers representing storage size will be
formatted as human readable strings with units, other fields are
converted to hexadecimal strings.

<!-- -->

`-v; --verbose`  
Emit more debug messages

# COPYRIGHT

Copyright © 2016 - 2020, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[ndctl-list](ndctl-list.md)
