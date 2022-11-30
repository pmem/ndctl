---
title: CXL Collab Sync
layout: page
---
# CXL Linux Sync: Ground Rules

* Do not share confidential information
* Do not share confidential product details
* Do not disclose CXL consortium confidential information
* Do discuss any Linux questions about **released** CXL specifications:
  * [https://www.computeexpresslink.org/spec-landing](https://www.computeexpresslink.org/spec-landing)
* Do use IRC as a supplement for this sync meeting
  * `#cxl` on `irc.oftc.net`

## November 2022

# Agenda 11/29
* Opens:
  * FSDAX ->notify_failure() regression work still pending
  * Others?
* Fixes merged for v6.1-rc4
* v6.2 merge window status
* Post v6.2 Features

# v6.1-rc4 Fixes

[https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tag/?h=v6.1-rc4](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tag/?h=v6.1-rc4)

Merged:

* Mailbox input payload fix
* Decoder commit crash
* LSA payload handling fix
* CFMWS NUMA Node setup
* Fix switch attached to single-port host-bridge
* BUG in create-region when no more intermediate port decoders available
* Fix region object memory leak
* Fix memdev object memory leak
* cxl_pmem static analysis fix

# v6.2 Merge Window Status

Merged:
* Cache invalidation for region physical invalidation scenarios
* DOE kernel/user access collision detection
* RCH preparation patches

In the queue (has review):
* RCH Support (including DVSEC Range Register enumeration)
* Security commands (including background commands)
* CXL Events to Linux Trace Events (including interrupts)
* RAS Capability Tracing on RCH and VH AER events

In the queue (needs review):
* XOR Interleave Math support
* Forward and reverse address translation (DPA <==> HPA)
* Poison retrieval
* cxl_pmem_wq removal
* EFI CPER record parsing for CXL error records

At risk:
* Volatile Region Discovery
* Volatile Region Provisioning
  * CDAT + QTG _DSM integration
* Poison inject and clear
* CXL perf monitoring

# Post v6.2 Features
* MLD Mailbox support for DCD event injection
* Dynamic Capacity Device support
  * Sparse DAX Region infrastructure
  * DCD event plumbing
* Switch mailbox CCI
  * Multi-head device mailbox tunneling
* Standalone CXL IDE
  * PCIE SPDM pre-requisite
* Maintenance Feature Support (DRAM PPR)
* Default "Soft Reserved" (EFI_MEMORY_SP) handling policy (cxl-cli + daxctl)


## October 2022

# Agenda 10/25
* Opens:
  * FSDAX page reference counting rework (merged in mm-unstable)
  * FSDAX ->notify_failure() regression work still pending
  * Code First ECR: ['SP' attribute in SRAT](https://bugzilla.tianocore.org/show_bug.cgi?id=4062)
  * QEMU emulation status update
  * Others?
* Fixes pending for v6.1-rc
* Features in flight for v6.2
* Rough plans for post v6.2 work

# v6.1 Fixes

[https://git.kernel.org/pub/scm/linux/kernel/git/cxl/cxl.git/log/?h=fixes](https://git.kernel.org/pub/scm/linux/kernel/git/cxl/cxl.git/log/?h=fixes)

Queued:

* Mailbox input payload fix
* Decoder commit crash
* LSA payload handling fix
* CFMWS NUMA Node setup

Pending:

* Fix switch attached to single-port host-bridge
* BUG in create-region when no more intermediate port decoders available

# v6.2 Features

In rough priority order, feedback welcome:

* RCH Support (including DVSEC Range Register enumeration)
* Cache invalidation for region physical invalidation scenarios
* RAS Capability Tracing on RCH and VH AER events
* CXL Events to Linux Trace Events (including interrupts)
* EFI CPER record parsing for CXL error records
* Forward and reverse address translation (DPA <==> HPA)
* Volatile Region Discovery
* Volatile Region Provisioning
* Security commands (including background commands)
* CXL perf monitoring
* Miscellaneous cleanups and renames

# Post v6.2 Features

* Dynamic Capacity Device support
  * Sparse DAX Region infrastructure
  * DCD event plumbing
* Maintenance Feature Support (DRAM PPR)
* Switch mailbox CCI
  * Multi-head device mailbox tunneling
* Default "Soft Reserved" (EFI_MEMORY_SP) handling policy (cxl-cli + daxctl)


## August 2022

# Agenda 8/30

* Opens:
  * FSDAX ->notify_failure() fixes
  * FSDAX page reference counting rework
* Linux v6.0-rc1 and ndctl (ndctl, daxctl, cxl-cli) v74 released
* Fix and Feature queue for v6.0-rc, v6.1 and ndctl-v75
* Rough plans for post v6.1 work for CXL 3.0 enabling

# Recently released

* Kernel:
  * DPA Space Accounting
  * PMEM Region Provisioning
  * DOE Support in PCI core
  * CDAT retrieval (for debug)
* User tooling:
  * cxl create-region
  * cxl reserve/free-dpa
  * cxl list -vvv

# Next fixes and features

* 'arch_flush_memregion()'
* Fix validation of x1 switch topologies
* Volatile region provisioning
* Region labels
* Security commands support
* Trace events for CXL events (including interrupts)
* 'cxl monitor' command
* CXL AER handling
* Address translation

# Future work

* Performance monitoring
* Maintenance Feature Support (DRAM PPR)
* Dynamic Capacity Device support
* Default "Soft Reserved" (EFI_MEMORY_SP) handling policy

## July 2022

# Agenda 7/26

* Opens:
  * FSDAX page reference counting rework
* What is queued for v6.0 (and ndctl-v74)?
* Late v6.0 updates
* Post v6.0 work

# Queued for v6.0

* DOE Support in PCI core
* CDAT retrieval (for debug)
* DPA Space Accounting
* PMEM Region Provisioning

# In review for v6.0

* Interleave granularity fixes
  * Fix host-bridge x1 interleave constraint
  * [Fix region granularity > host-bridge granularity handling](https://lore.kernel.org/linux-cxl/165853778028.2430596.7493880465382850752.stgit@dwillia2-xfh.jf.intel.com/) (scale factors must match)

# Post v6.0 material

* Pre-existing region enumeration
* Volatile region provisioning
* XORMAP interleave support
* Trace Events for CXL Events
* List Poison
* Scan Media
* Address translation
* Region persistence in labels
* Region enumeration via labels

## June 2022

# Agenda: 6/28

* Opens:
  * CXL Device Tree Support
  * MEM_HWINIT_MODE=0
  * QEMU mainline CXL support is live
* What is in review for v5.20 (and ndctl-v74)
* What else might make v5.20?
* What is post v5.20 material?

# v5.20 in review

* [DOE + CDAT](https://lore.kernel.org/linux-cxl/20220628041527.742333-1-ira.weiny@intel.com/)
* [PMEM Region Provisioning](https://lore.kernel.org/linux-cxl/165603869943.551046.3498980330327696732.stgit@dwillia2-xfh/)

# v5.20 on deck

* Pre-existing region enumeration
* Region persistence in labels
* Region enumeration via labels
* Address translation foundation

# Post v5.20 material

* List Poison
* Scan Media
* XORMAP interleave support
* Trace Events for CXL Events
* Address translation (in cxl-cli) for all kernel supported Events, List Poison, and Scan Media

## May 2022

# Agenda: 5/31

* What is in v5.19?
* What is on deck for v5.20?
* What is post v5.20 material?
* Opens

# v5.19 / ndctl-v73

* Kernel
  * lockdep annotations
  * CXL _OSC (native CXL hotplug + error "handling")
  * Disable suspend
  * Mem_enable fixes

# v5.20 / ndctl-v74

* Kernel
  * Region Provisioning
  * DOE Core
  * CXL CDAT Retrieval
  * Event record handling core
    * Scan Media records
    * Event Interrupts
    * Background command timesharing
* Userpace
  * 'cxl create-region'
  * Region listing support
  * Scan media / Event records to json
  * Address translation

# Post v5.20 / v6.0

* Kernel
  * SPDM Attestation
  * IDE
  * Security commands
* Userspace
  * Attestation helper process
  * CXL Device-DAX Policy
