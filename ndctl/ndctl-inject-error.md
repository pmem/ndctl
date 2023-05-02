---
layout: page
---

# NAME

ndctl-inject-error - inject media errors at a namespace offset

# SYNOPSIS

>     ndctl inject-error <namespace> [<options>]

# THEORY OF OPERATION

The capacity of an NVDIMM REGION (contiguous span of persistent memory)
is accessed via one or more NAMESPACE devices. REGION is the Linux term
for what ACPI and UEFI call a DIMM-interleave-set, or a
system-physical-address-range that is striped (by the memory controller)
across one or more memory modules.

The UEFI specification defines the *NVDIMM Label Protocol* as the
combination of label area access methods and a data format for
provisioning one or more NAMESPACE objects from a REGION. Note that
label support is optional and if Linux does not detect the label
capability it will automatically instantiate a "label-less" namespace
per region. Examples of label-less namespaces are the ones created by
the kernel’s *memmap=ss!nn* command line option (see the nvdimm wiki on
kernel.org), or NVDIMMs without a valid *namespace index* in their label
area.

<div class="note">

Label-less namespaces lack many of the features of their label-rich
cousins. For example, their size cannot be modified, or they cannot be
fully *destroyed* (i.e. the space reclaimed). A destroy operation will
zero any mode-specific metadata. Finally, for create-namespace
operations on label-less namespaces, ndctl bypasses the region capacity
availability checks, and always satisfies the request using the full
region capacity. The only reconfiguration operation supported on a
label-less namespace is changing its *mode*.

</div>

A namespace can be provisioned to operate in one of 4 modes, *fsdax*,
*devdax*, *sector*, and *raw*. Here are the expected usage models for
these modes:

- fsdax: Filesystem-DAX mode is the default mode of a namespace when
  specifying *ndctl create-namespace* with no options. It creates a
  block device (/dev/pmemX\[.Y\]) that supports the DAX capabilities of
  Linux filesystems (xfs and ext4 to date). DAX removes the page cache
  from the I/O path and allows mmap(2) to establish direct mappings to
  persistent memory media. The DAX capability enables workloads /
  working-sets that would exceed the capacity of the page cache to scale
  up to the capacity of persistent memory. Workloads that fit in page
  cache or perform bulk data transfers may not see benefit from DAX.
  When in doubt, pick this mode.

- devdax: Device-DAX mode enables similar mmap(2) DAX mapping
  capabilities as Filesystem-DAX. However, instead of a block-device
  that can support a DAX-enabled filesystem, this mode emits a single
  character device file (/dev/daxX.Y). Use this mode to assign
  persistent memory to a virtual-machine, register persistent memory for
  RDMA, or when gigantic mappings are needed.

- sector: Use this mode to host legacy filesystems that do not checksum
  metadata or applications that are not prepared for torn sectors after
  a crash. Expected usage for this mode is for small boot volumes. This
  mode is compatible with other operating systems.

- raw: Raw mode is effectively just a memory disk that does not support
  DAX. Typically this indicates a namespace that was created by tooling
  or another operating system that did not know how to create a Linux
  *fsdax* or *devdax* mode namespace. This mode is compatible with other
  operating systems, but again, does not support DAX operation.

ndctl-inject-error can be used to ask the platform to simulate media
errors in the NVDIMM address space to aid debugging and development of
features related to error handling.

By default, injecting an error actually only injects an error to the
first *n* bytes of the block, where *n* is the output of
ndctl_cmd_ars_cap_get_size(). In other words, we only inject one
*ars_unit* per sector. This is sufficient for Linux to mark the whole
sector as bad, and will show up as such in the various *badblocks* lists
in the kernel. If multiple blocks are being injected, only the first *n*
bytes of each block specified will be injected as errors. This can be
overridden by the --saturate option, which will force the entire block
to be injected as an error.

<div class="warning">

These commands are DANGEROUS and can cause data loss. They are only
provided for testing and debugging purposes.

</div>

# EXAMPLES

Inject errors in namespace0.0 at block 12 for 2 blocks (i.e. 12, 13)

>     ndctl inject-error --block=12 --count=2 namespace0.0

Check status of injected errors on namespace0.0

>     ndctl inject-error --status namespace0.0

Uninject errors at block 12 for 2 blocks on namespace0.0

>     ndctl inject-error --uninject --block=12 --count=2 namespace0.0

# OPTIONS

`-B; --block=`  
Namespace block offset in 512 byte sized blocks where the error is to be
injected.

    NOTE: The offset is interpreted in different ways based on the "mode"
    of the namespace. For "raw" mode, the offset is the base namespace
    offset. For "fsdax" mode (i.e. a "pfn" namespace), the offset is
    relative to the user-visible part of the namespace, and the offset
    introduced by the kernel's metadata will be accounted for. For a
    "sector" mode namespace (i.e. a "BTT" namespace), the offset is
    relative to the base namespace, as the BTT translation details are
    internal to the kernel, and can't be accounted for while injecting
    errors.

`-n; --count=`  
Number of blocks to inject as errors. This is also in terms of fixed,
512 byte blocks.

`-d; --uninject`  
This option will ask the platform to remove any injected errors for the
specified block offset, and count.

    WARNING: This will not clear the kernel's internal badblock tracking,
    those can only be cleared by doing a write to the affected locations.
    Hence use the --clear option only if you know exactly what you are
    doing. For normal usage, injected errors should only be cleared by
    doing writes. Do not expect have the original data intact after
    injecting an error, and clearing it using --clear - it will be lost,
    as the only "real" way to clear the error location is to write to it
    or zero it (truncate/hole-punch).

`-t; --status`  
This option will retrieve the status of injected errors. Note that this
will not retrieve all known/latent errors (i.e. non injected ones), and
is NOT equivalent to performing an Address Range Scrub.

`-N; --no-notify`  
This option is only valid when injecting errors. By default, the error
inject command and will ask platform firmware to trigger a notification
in the kernel, asking it to update its state of known errors. With this
option, the error will still be injected, the kernel will not get a
notification, and the error will appear as a latent media error when the
location is accessed. If the platform firmware does not support this
feature, this will have no effect.

`-S; --saturate`  
This option forces error injection or un-injection to cover the entire
address range covered by the specified block(s).

`-v; --verbose`  
Emit debug messages for the error injection process

<!-- -->

`-u; --human`  
Format numbers representing storage sizes, or offsets as human readable
strings with units instead of the default machine-friendly raw-integer
data. Convert other numeric fields into hexadecimal strings.

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

# COPYRIGHT

Copyright © 2016 - 2022, Intel Corporation. License GPLv2: GNU GPL
version 2 <http://gnu.org/licenses/gpl.html>. This is free software: you
are free to change and redistribute it. There is NO WARRANTY, to the
extent permitted by law.

# SEE ALSO

`ndctl-list(1)` ,
