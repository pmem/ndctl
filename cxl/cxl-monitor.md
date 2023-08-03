---
layout: page
---

# NAME

cxl-monitor - Monitor the CXL trace events

# SYNOPSIS

>     cxl monitor [<options>]

# DESCRIPTION

cxl-monitor is used for monitoring the CXL trace events emitted by the
kernel and convert them to json objects and dumping the json format
notifications to standard output or a logfile.

# EXAMPLES

Run a monitor as a daemon to monitor events and output to a log file.

>     cxl monitor --daemon --log=/var/log/cxl-monitor.log

Run a monitor as a one-shot command and output the notifications to
stdio.

>     cxl monitor

Run a monitor daemon as a system service

>     systemctl start cxl-monitor.service

# OPTIONS

`-l; --log=`  
Send log messages to the specified destination.

- "\<file\>": Send log messages to specified \<file\>.

- "standard": Send messages to standard output.

The default log destination is */var/log/cxl-monitor.log* if "--daemon"
is specified, otherwise *standard*. Note that standard and relative path
for \<file\> will not work if "--daemon" is specified.

`--daemon`  
Run a monitor as a daemon.

<!-- -->

`-v; --verbose`  
Emit more debug messages

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

[cxl-list](cxl-list)
