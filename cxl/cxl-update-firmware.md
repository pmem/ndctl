---
layout: page
---

# NAME

cxl-update-firmware - update the firmware on a CXL memdev

# SYNOPSIS

>     cxl update-firmware <mem0> [<mem1>..<memN>] [<options>]

# DESCRIPTION

Update the firmware on one or more CXL mem devices. The mem devices must
support the set of firmware related mailbox commands.

This command doesn’t directly issue the transfer / activate firmware
mailbox commands. Instead, the kernel’s firmware loader facility is used
to provide the kernel with the data, and the kernel handles performing
the actual update while also managing time slicing the transfer w.r.t.
other background commands.

# EXAMPLE

    # cxl update-firmware mem0 -F firmware.bin -w
    [
      {
        "memdev":"mem0",
        "pmem_size":1073741824,
        "ram_size":1073741824,
        "serial":0,
        "numa_node":0,
        "host":"cxl_mem.0",
        "firmware":{
          "num_slots":3,
          "active_slot":2,
          "online_activate_capable":false,
          "slot_1_version":"cxl_test_fw_001",
          "slot_2_version":"cxl_test_fw_002",
          "slot_3_version":"cxl_test_new_fw",
          "fw_update_in_progress":false
        }
      }
    ]
    firmware update completed on 1 mem device

# OPTIONS

`-b; --bus=`  
Restrict the operation to the specified bus.

`-F; --firmware-file`  
Firmware image file to use for the update.

`-c; --cancel`  
Attempt to abort an in-progress firmware update. This may fail depending
on what stage the update process is in.

`-w; --wait`  
By default, the update-firmware command only initiates the firmware
update, and returns, while the update operation happens asynchronously
in the background. This option makes the firmware update command
synchronous by waiting for it to complete before returning.

    If --wait is passed in without an accompanying firmware-file,
    it will initiate a wait on any current in-progress firmware
    updates, and then return.

<!-- -->

`-v; --verbose`  
Emit more debug messages

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

[cxl-list](cxl-list),
