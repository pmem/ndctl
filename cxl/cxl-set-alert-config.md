---
layout: page
---

# NAME

cxl-set-alert-config - set the warning alert threshold on a CXL memdev

# SYNOPSIS

>     cxl set-alert-config <mem0> [<mem1>..<memN>] [<options>]

# DESCRIPTION

CXL device raises an alert when its health status is changed. Critical
alert shall automatically be configured by the device after a device
reset. If supported, programmable warning thresholds also be initialized
to vendor recommended defaults, then could be configured by the user.

Use this command to configure warning alert thresholds of a device.
Having issued this command, the newly requested warning thresholds would
override the previously programmed warning thresholds.

To enable warning alert, set both *threshold=value* and *alert=on*. To
disable warning alert, set only *alert=off*. Other cases would cause
errors.

Use "cxl list -m \<memdev\> -A" to examine the programming warning
threshold capabilities of a device.

# EXAMPLES

Set warning threshold to 30 and enable alert for life used.

    # cxl set-alert-config mem0 --life-used-threshold=30 --life-used-alert=on
    {
      "memdev":"mem0",
      "ram_size":"1024.00 MiB (1073.74 MB)",
      "alert_config":{
        "life_used_prog_warn_threshold_valid":true,
        "dev_over_temperature_prog_warn_threshold_valid":false,
        "dev_under_temperature_prog_warn_threshold_valid":false,
        "corrected_volatile_mem_err_prog_warn_threshold_valid":false,
        "corrected_pmem_err_prog_warn_threshold_valid":false,
        "life_used_prog_warn_threshold_writable":true,
        "dev_over_temperature_prog_warn_threshold_writable":true,
        "dev_under_temperature_prog_warn_threshold_writable":true,
        "corrected_volatile_mem_err_prog_warn_threshold_writable":true,
        "corrected_pmem_err_prog_warn_threshold_writable":true,
        "life_used_crit_alert_threshold":75,
        "life_used_prog_warn_threshold":30,
        "dev_over_temperature_crit_alert_threshold":0,
        "dev_under_temperature_crit_alert_threshold":0,
        "dev_over_temperature_prog_warn_threshold":0,
        "dev_under_temperature_prog_warn_threshold":0,
        "corrected_volatile_mem_err_prog_warn_threshold":0,
        "corrected_pmem_err_prog_warn_threshold":0
      },
      "serial":"0",
      "host":"0000:0d:00.0"
    }
    cxl memdev: cmd_set_alert_config: set alert configuration 1 mem

Disable warning alert for life_used.

    # cxl set-alert-config mem0 --life-used-alert=off
    {
      "memdev":"mem0",
      "ram_size":"1024.00 MiB (1073.74 MB)",
      "alert_config":{
        "life_used_prog_warn_threshold_valid":false,
        "dev_over_temperature_prog_warn_threshold_valid":false,
        "dev_under_temperature_prog_warn_threshold_valid":false,
        "corrected_volatile_mem_err_prog_warn_threshold_valid":false,
        "corrected_pmem_err_prog_warn_threshold_valid":false,
        "life_used_prog_warn_threshold_writable":true,
        "dev_over_temperature_prog_warn_threshold_writable":true,
        "dev_under_temperature_prog_warn_threshold_writable":true,
        "corrected_volatile_mem_err_prog_warn_threshold_writable":true,
        "corrected_pmem_err_prog_warn_threshold_writable":true,
        "life_used_crit_alert_threshold":75,
        "life_used_prog_warn_threshold":30,
        "dev_over_temperature_crit_alert_threshold":0,
        "dev_under_temperature_crit_alert_threshold":0,
        "dev_over_temperature_prog_warn_threshold":0,
        "dev_under_temperature_prog_warn_threshold":0,
        "corrected_volatile_mem_err_prog_warn_threshold":0,
        "corrected_pmem_err_prog_warn_threshold":0
      },
      "serial":"0",
      "host":"0000:0d:00.0"
    }
    cxl memdev: cmd_set_alert_config: set alert configuration 1 mem

# OPTIONS

\<memory device(s)\>  
A *memX* device name, or a memdev id number. Restrict the operation to
the specified memdev(s). The keyword *all* can be specified to indicate
the lack of any restriction.

`-S; --serial`  
Rather an a memdev id number, interpret the \<memdev\> argument(s) as a
list of serial numbers.

`-L; --life-used-threshold=`  
Set \<value\> for the life used warning alert threshold.

`--life-used-alert=`  
Enable or disable the life used warning alert. Options are *on* or
*off*.

`-O; --over-temperature-threshold=`  
Set \<value\> for the device over temperature warning alert threshold.

`--over-temperature-alert=`  
Enable or disable the device over temperature warning alert. Options are
*on* or *off*.

`-U; --under-temperature-threshold=`  
Set \<value\> for the device under temperature warning alert threshold.

`--under-temperature-alert=`  
Enable or disable the device under temperature warning alert. Options
are *on* or *off*.

`-V; --volatile-mem-err-threshold=`  
Set \<value\> for the corrected volatile memory error warning alert
threshold.

`--volatile-mem-err-alert=`  
Enable or disable the corrected volatile memory error warning alert.
Options are *on* or *off*.

`-P; --pmem-err-threshold=`  
Set \<value\> for the corrected persistent memory error warning alert
threshold.

`--pmem-err-alert=`  
Enable or disable the corrected persistent memory error warning alert.
Options are *on* or *off*.

`-v; --verbose`  
Turn on verbose debug messages in the library (if libcxl was built with
logging and debug enabled).

# SEE ALSO

CXL-3.0 8.2.9.8.3.3
