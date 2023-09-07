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
* Do use IRC as a supplement for this sync meeting for quick questions
  * `#cxl` on `irc.oftc.net`
* Do follow-up on linux-cxl@vger.kernel.org for longer questions / debug
* https://pmem.io/ndctl/collab/

# September 2023
* Opens:
  * John: CXL memory online by default memhp_default_state=offline not working?
* QEMU
* cxl-cli
* v6.6 Fixes
* v6.7 Queue

## QEMU
* Merge window induced slowness
* Round-up of fixlets sent up
* Multiple HDM Decoder support for endpoints posted
* Serial number update
* Maintainer feedback administrivia cleanups
* Sort out revision numbers for spec version comments
  * advocate with your rep about caching old copies at spec-landing
* MCTP I2C from NVME
  * Single Aspeed i2c controller driver has support
  * POC quality / out-of-tree support until server class driver arrives
* DCD Update
  * waiting for kernel-side code resolution
  * Get Extent List for unaccepted memory, track pending state in the implementation
  * cxl-test may need updates too
* MHD Update
  * Joint effort with SK Hynix, custom command set
  * Proto-DCD
  * Single logical device
  * Software Development Vehicle
* CPMU, ARM, Compliance, Type-2
* SPDM Interest
  * WDC looking at library-izing it, still looking to support and external agent
* FM API (MCTP Mailboxes + Switch CCI + MHD Mailbox)


## v6.6 Fixes
* CXL RAS Enabling 
* [Region Granularity Setup](https://patchwork.kernel.org/project/cxl/patch/20230822180928.117596-1-alison.schofield@intel.com/)
* [Region Decoder Discover](https://patchwork.kernel.org/project/cxl/patch/20230822014303.110509-1-alison.schofield@intel.com/)

## v6.7 Queue
* RCH EH (under)
* Kernel SPDM
  * WDC showing up to help
  * Invite to CXL sync? Invited to "devsec"

# August 2023
* Opens:
  * [Linux Plumbers CXL Microconference CFP](http://lore.kernel.org/r/a4c2gx2tnm4ckax7qkx2trnvmqjssfytc45sb2zikuayd2marc@rpsjp4icgsvn)
      * uConf proposals close at end of the August

## QEMU Update
  * Not a huge amount going in this merge, doc, fixes Multiple HDM decoders should be going in this merge.
    * Lot of stuff is backed up by the mailbox rework
    * Jonathans gitlab has [DCD preview queued up](https://gitlab.com/jic23/qemu).
      * Ira did some testing and fixes were merged in latest version
      * Jonathan might have broken it with rebasing. So just a reminder that this is work in progress.
    * MCTP support over I2c...  Support is coming from NVME-MI this work is similar to FM-API

## cxl-cli update:
* [v78 release](http://lore.kernel.org/r/8a83f1832c95e327a4695b607729102216a3e2f0.camel@intel.com)
* [Hotplug helper proposal](http://lore.kernel.org/r/20230807213635.3633907-1-terry.bowman@amd.com)

## v6.5 Fixes Queue
* [rc4 updates](https://git.kernel.org/pub/scm/linux/kernel/git/cxl/cxl.git/tag/?h=cxl-fixes-6.5-rc4)
* [rc5 updates](https://git.kernel.org/pub/scm/linux/kernel/git/cxl/cxl.git/tag/?h=cxl-fixes-6.5-rc5)

## v6.6 Queue
* RCH Error handling
  * Terry working on it right now. Was waiting on response from Dan which should be there yesterday.
  * Will pick that work back up

* Type2
  * Davidlohr to submit the fix for type2 init collision. (Merged)
  * Dan rebasing patches.  There is conflict here with the Switch CCI work.  See below.

* DCD 
  * Ira is reworking the patch set quite a bit.
  * Fan’s QEMU DCD work is being used
  * Cxl-test being added for better regression testing
  * Cxl-test event processing was changed
  * New DAX device work needed to handle sparse extents within the dax region
  * Interleaving is in the back of his head and Navneet has been looking into this. However, interleaving is not slated for this initial work
  * Jonathan - concerned that interleaving should not to be precluded
  * Leave in comments about where interleaving would fit in.
  * Interleaving is the next major feature…
  * QEMU - DCD merge would be at least 6.7 aligned.

* Switch CCI (Jonathan)
  * Opens around what we do for user space – almost every command is destructive
  * Maybe just CXL raw commands are required?
  * Patch set has been a pain to rebase on type 2 from Dan
  * Would really like review / feedback
  * Davidlohr would like to merge the ‘moving around code’ sooner
  * Would help with the type 2 conflicts
  * It is hard to generalize the code without this second user
  * Not critical for 6.6
  * would like to see an early merge slated for 6.7
  * In the end – Security questions are major gating factor

* Memory tiring in general
  * CDAT vs HMAT
  * ‘Distance’ calculations vary
  *  Patch set: ‘Mem tiring calculating abstract distance from ACPI’ (v6.7 material)

## FM general topics
  * We said we would talk about FM things in this meeting…
  * Is there something at plumbers?  Yes there is.
  * Plumbers BoF for FM stuff?
	
## Question from discord:
* John: "numa ratio policy patch?"
  * Jonathan will try and dig in to see where the patches are
  * We are talking at a VMA level.



## QEMU Update

## cxl-cli update
* [Hotplug helper, and expanding cxl-cli beyond C](http://lore.kernel.org/r/20230807213635.3633907-1-terry.bowman@amd.com)
  
## v6.5 Fixes Queue
* Region autodiscovery fixes
  * [x1 granularity calculation fix](https://patchwork.kernel.org/project/cxl/list/?series=773298): minor fixups requested
  * [switch decoder allocation](https://patchwork.kernel.org/project/cxl/list/?series=773274): minor fixups requested
* Hotplug fixes
  * [Cleanup softreserve on takeover](https://patchwork.kernel.org/project/cxl/list/?series=773250): awaiting review
  * [Reuse SRAT proximity domain](https://patchwork.kernel.org/project/cxl/list/?series=764146): pinged x86
* [CXL _OSC AER Fixup](https://patchwork.kernel.org/project/cxl/list/?series=772827): minor fixups requested

## v6.6 Queue
* Queue closes August 18th
* [RCH Error handling](https://patchwork.kernel.org/project/cxl/list/?series=761698): fixes requested
* QTG enabling
  * [ACPI HMAT Generic Port support](https://patchwork.kernel.org/project/cxl/list/?series=759643): awaiting merge
  * [Surface QTG ID info](https://patchwork.kernel.org/project/cxl/list/?series=758023): awaiting merge
  * [CDAT Parsing](https://patchwork.kernel.org/project/cxl/list/?series=757264): awaiting merge
* Finish Type2 enabling
  * [Fix security init collision](https://patchwork.kernel.org/project/cxl/list/?series=770745): different approach requested
  * [Rebase remaining Type2 HDM API](http://lore.kernel.org/r/168592149709.1948938.8663425987110396027.stgit@dwillia2-xfh.jf.intel.com)
* [DCD](https://patchwork.kernel.org/project/cxl/list/?series=757239): awaiting next rev
* [Switch CCI](https://patchwork.kernel.org/project/cxl/list/?series=773085): awaiting review

# July 2023

## ndctl / cxl-cli update
* v78 - minor fixups only - will go out this week
* v79
  * Firmware update (no outstanding comments)
  * Poison injection (awaiting new rev)
  * Others?

  ...further notes not captured.

# June 2023
* Opens:
  * OpenBMC collaboration
  * Labels / Persistent Naming (6.3 issue?)
  * Add a CXL-CLI Item to the Agenda
* QEMU Update
* v6.4 Fixes
* v6.5 Merge Queue
* Post v6.5 material

## QEMU Update
* QEMU DCD Support?
* MLD Support
* CCI layering work for OpenBMC collab
* I2C ACPI aspeed controller (upstream questionable)

## v6.4 Fixes
* [DAX Use After Free](https://lore.kernel.org/linux-cxl/168577282846.1672036.13848242151310480001.stgit@dwillia2-xfh.jf.intel.com/)
* [SRAT vs CFMWS Fixup](https://lore.kernel.org/linux-cxl/cover.1684448934.git.alison.schofield@intel.com/)(pending next rev and x86 review)
* [Cache Management Discussion](http://lore.kernel.org/r/648220cdade2_1433ac2949b@dwillia2-xfh.jf.intel.com.notmuch)

## v6.5 Merge Queue
* [RCH Error Handling](https://lore.kernel.org/linux-cxl/20230607221651.2454764-1-terry.bowman@amd.com/)(awaiting v6 posting)
  * Follow-up: RDPAS vs Root Port Scanning?
* [Background command support](http://lore.kernel.org/r/20230421092321.12741-1-dave@stgolabs.net/)(baseline pushed, awaiting consumer)
  * [Sanitization](http://lore.kernel.org/r/20230612181038.14421-1-dave@stgolabs.net)(pending review)
  * [Firmware udpate](http://lore.kernel.org/r/20230602-vv-fw_update-v3-0-869f82069c95@intel.com)(awaiting final review)
* [CXL perf monitoring](http://lore.kernel.org/r/20230303175022.10806-1-Jonathan.Cameron@huawei.com)(awaiting push to cxl-next)

## Post v6.5
* [QoS Class support](http://lore.kernel.org/r/168382784460.3510737.9571643715488757272.stgit@djiang5-mobl3)(pre-reqs heading for v6.5)
  * [CDAT + QTG _DSM integration](http://lore.kernel.org/r/168088732996.1441063.10107817505475386072.stgit@djiang5-mobl3)(pending review)
* Standalone CXL IDE
  * [PCIE SPDM pre-requisite](https://github.com/l1k/linux/commits/doe)
  * [KEYP table enabling](https://cdrdv2-public.intel.com/732838/732838_Root%20Complex%20IDE%20Programming%20Guide_Rev1p0.pdf)
* [Switch CCI](http://lore.kernel.org/r/20221025104243.20836-1-Jonathan.Cameron@huawei.com)
* memory_failure() for CXL events
* [Type-2 Region Creation](http://lore.kernel.org/r/168592149709.1948938.8663425987110396027.stgit@dwillia2-xfh.jf.intel.com) (awaiting review)
* Scan Media
  * background dependency
* [Dynamic Capacity Device support](https://git.kernel.org/pub/scm/linux/kernel/git/cxl/cxl.git/log/?h=for-6.5/dcd-preview)(awaiting next rev)
  * Sparse DAX Region infrastructure
  * DCD event plumbing


# May 2023
* Opens:
  * rasdaemon patches need review
* LSF/MM takeaways
* QEMU Update
* v6.4 pull summary
* v6.5 Queue

## LSF/MM takeaways
* CXL 3.0 specification update review well received
* Discussed nodes vs zones and mempolicy vs mmap flags, nodes+mempolicy continues as the path forward
* Fabric manager: several efforts in flight (one in rust one in golang, OCP and OFA efforts as well)
* Live migration: CXL as a transport for migration, opportunity for migrate in place

## QEMU Update
* Several patchkits ready and awaiting final merge:
  * volatile memory
  * poison handling
  * events
* DCD support starting to surface
  * Initial test results of the pre-RFC implementation look good
  * QMP based interface

## v6.4 pull summary
* [DOE rework](https://lore.kernel.org/all/cover.1678543498.git.lukas@wunner.de/)(queued)
* [Poison retrieval](http://lore.kernel.org/r/cover.1679284567.git.alison.schofield@intel.com)(pending review)
  * Forward and reverse address translation (DPA <==> HPA)
* [Poison inject and clear](http://lore.kernel.org/r/cover.1678471465.git.alison.schofield@intel.com)(awaiting next rev)

## v6.5 queue
* [Background command support](http://lore.kernel.org/r/20230421092321.12741-1-dave@stgolabs.net/)(pending review)
* [QoS Class support](http://lore.kernel.org/r/168382784460.3510737.9571643715488757272.stgit@djiang5-mobl3)(pending review)
* [CDAT + QTG _DSM integration](http://lore.kernel.org/r/168088732996.1441063.10107817505475386072.stgit@djiang5-mobl3)(pending review)
* [CXL perf monitoring](http://lore.kernel.org/r/20230303175022.10806-1-Jonathan.Cameron@huawei.com)(awaiting perf acks)
* [Dynamic Capacity Device support](https://git.kernel.org/pub/scm/linux/kernel/git/cxl/cxl.git/log/?h=for-6.5/dcd-preview)(awaiting next rev)
  * Sparse DAX Region infrastructure
  * DCD event plumbing
* [Firmware udpate](http://lore.kernel.org/r/20230421-vv-fw_update-v1-0-22468747d72f@intel.com))(pending review)
  * v2 posted with review feedback incorporated
  * man page added to the cxl-cli patchkit
* [RAS Capability Tracing on RCH AER events](http://lore.kernel.org/r/20221021185615.605233-1-terry.bowman@amd.com)(awaiting next rev)
* Standalone CXL IDE
  * PCIE SPDM pre-requisite
  * [KEYP table enabling](https://cdrdv2-public.intel.com/732838/732838_Root%20Complex%20IDE%20Programming%20Guide_Rev1p0.pdf)
* [Switch CCI](http://lore.kernel.org/r/20221025104243.20836-1-Jonathan.Cameron@huawei.com)
* memory_failure() for CXL events
* [Type-2 Region Creation](https://git.kernel.org/pub/scm/linux/kernel/git/cxl/cxl.git/log/?h=for-6.4/cxl-type-2)(awaiting first rev)
* Scan Media
  * background dependency



# April 2023
* Opens:
* QEMU Update
* v6.3 Fixes
* v6.4 Queue
* v6.5 Queue

## v6.3 Fixes
* [Decoder Enumeration Fixes](http://lore.kernel.org/r/168149842935.792294.13212627946146993066.stgit@dwillia2-xfh.jf.intel.com)(queued)

## v6.4 Queue
* [DOE rework](https://lore.kernel.org/all/cover.1678543498.git.lukas@wunner.de/)(queued)
* [Poison retrieval](http://lore.kernel.org/r/cover.1679284567.git.alison.schofield@intel.com)(pending review)
  * Forward and reverse address translation (DPA <==> HPA)
* [Poison inject and clear](http://lore.kernel.org/r/cover.1678471465.git.alison.schofield@intel.com)(awaiting next rev)
* [CXL perf monitoring](http://lore.kernel.org/r/20230303175022.10806-1-Jonathan.Cameron@huawei.com)(awaiting perf acks)



## v6.5 Queue
* [CDAT + QTG _DSM integration](http://lore.kernel.org/r/168088732996.1441063.10107817505475386072.stgit@djiang5-mobl3)(review pending)
* [Dynamic Capacity Device support](https://git.kernel.org/pub/scm/linux/kernel/git/cxl/cxl.git/log/?h=for-6.5/dcd-preview)(awaiting next rev)
  * Sparse DAX Region infrastructure
  * DCD event plumbing
* Firmware Update (awaiting first rev)
* [RAS Capability Tracing on RCH AER events](http://lore.kernel.org/r/20221021185615.605233-1-terry.bowman@amd.com)(awaiting next rev)
* Standalone CXL IDE
  * PCIE SPDM pre-requisite
  * [KEYP table enabling](https://cdrdv2-public.intel.com/732838/732838_Root%20Complex%20IDE%20Programming%20Guide_Rev1p0.pdf)
* [Switch CCI](http://lore.kernel.org/r/20221025104243.20836-1-Jonathan.Cameron@huawei.com)
* memory_failure() for CXL events
* [Type-2 Region Creation](https://git.kernel.org/pub/scm/linux/kernel/git/cxl/cxl.git/log/?h=for-6.4/cxl-type-2)(awaiting first rev)
* Scan Media
  * background dependency


# March 2023
* Opens:
  * [cxl/hdm: Fix hdm decoder init by adding COMMIT field check](http://lore.kernel.org/r/20230228224014.1402545-1-fan.ni@samsung.com)
  * HDM-D/DB Kernel-internal region creation
* QEMU Update
* v6.4 Queue

## v6.4 Queue
* [DOE rework](https://lore.kernel.org/all/cover.1678543498.git.lukas@wunner.de/)
* [Poison retrieval](http://lore.kernel.org/r/cover.1679284567.git.alison.schofield@intel.com)
  * Forward and reverse address translation (DPA <==> HPA)
* [Poison inject and clear](http://lore.kernel.org/r/cover.1678471465.git.alison.schofield@intel.com)
* Scan Media
  * background dependency
* [Background command support](https://lore.kernel.org/all/20230224194652.1990604-1-dave@stgolabs.net/)
* Dynamic Capacity Device support
  * Sparse DAX Region infrastructure
  * DCD event plumbing
* Firmware Update
* CDAT + QTG _DSM integration
* [CXL perf monitoring](http://lore.kernel.org/r/20230303175022.10806-1-Jonathan.Cameron@huawei.com)
* [RAS Capability Tracing on RCH AER events](http://lore.kernel.org/r/20221021185615.605233-1-terry.bowman@amd.com)
* Standalone CXL IDE
  * PCIE SPDM pre-requisite
* [Switch CCI](http://lore.kernel.org/r/20221025104243.20836-1-Jonathan.Cameron@huawei.com)
* memory_failure() for CXL events
* Maintenance Feature Support (DRAM PPR) (BMC only?)

## Notes
* Question about kernel code modularity for accelerator drivers
  * Expectation is that it is a bug if CXL core code cannnot be reused for devices outside of the class-device definition
* DCD Sharing may be the first user of HDM-DB functionality in the kernel, QEMU model for this in scoping
* Multi-head (not yet MLD) device support in the works for QEMU
* QEMU gaining a fix for clearing the HDM decoder COMMITTED bit when deactivating decoders
* Poison
* Poison inject can be done unconditionally, rely on "injected" indication to delineate real vs simulated hardware problems
  * open question: should the driver taint the kernel on inject? No, ACPI EINJ does not
  * Poison list: emit trace event on inject event? Maybe already covered by another event record

# February 2023

* Opens:
  * CXL DVSEC emulation fixes
  * QEMU Update
* v6.3 Merge Window
* v6.4 Queue

## v6.3 Merge Window
* Move tracepoints to cxl_core
* Export CXL _OSC error control result
* CXL Events to Linux Trace Events (including interrupts)
* HDM decoder emulation
* Default "Soft Reserved" (EFI_MEMORY_SP) handling policy (kernel)
* Volatile Region Discovery
* Volatile Region Provisioning
* Set timestamp

## v6.4 Queue
* Poison inject and clear
* Forward and reverse address translation (DPA <==> HPA)
* Poison retrieval
* memory_failure() for CXL events
* Dynamic Capacity Device support
  * Sparse DAX Region infrastructure
  * DCD event plumbing
* CDAT + QTG _DSM integration
* DOE rework
* Standalone CXL IDE
  * PCIE SPDM pre-requisite
* RAS Capability Tracing on RCH AER events
* Maintenance Feature Support (DRAM PPR)
* CXL perf monitoring
* Switch CCI

## Notes:
* QEMU:
  * Several patch kits in flight: https://gitlab.com/jic23/qemu/-/commits/cxl-2023-02-21/

* AER Discussion:
  * What about CXL Reset for recovery?
    * May be more relevant for future Type-2 devices than Type-3
    * Add another PCI error recovery reset type?
    * Map FLR => CXL Reset?
    * PCI core supports per-device reset methods

* DCD
  * Look at MLD support before Switch CCI support

* CXL perf monitoring
  * https://lore.kernel.org/r/20221018121318.22385-1-Jonathan.Cameron@huawei.com

* FW Update
  * depends on background command support
  * revisit for v6.4

* Scan Media
  * revisit for v6.4

## January 2023

# Agenda 01/24
* Opens:
  * DAX-page request API rework
  * FM Project? LSF/MM topic
  * Type-3 volatile
  * QEMU Update
* v6.2 Merge Window
* v6.2-rc Fixes
* v6.3 Status
* v6.3+ Future Work

# v6.2 Merge Window
* Cache invalidation for region physical invalidation scenarios
* DOE kernel/user access collision detection
* RCH preparation patches
* RCH Support (including DVSEC Range Register enumeration)
* Security commands (including background commands)
* RAS Capability Tracing on VH AER events
* XOR Interleave Math support
* cxl_pmem_wq removal
* EFI CPER record parsing for CXL error records

# v6.2-rc Fixes
Merged in cxl/fixes:
* RAS UE addr mis-assignment

Pending merge:
* Fix nvdimm unregistration

# v6.3 Status
Merged in cxl/next:
* Move tracepoints to cxl_core
* Export CXL _OSC error control result

Pending merge:
* CXL Events to Linux Trace Events (including interrupts)
* Poison inject and clear
* Forward and reverse address translation (DPA <==> HPA)
* Poison retrieval
* HDM decoder emulation

Awaiting next (or first) posting:
* RAS Capability Tracing on RCH AER events
* Volatile Region Discovery
* Volatile Region Provisioning
* CDAT + QTG _DSM integration
* Set timestamp
* memory_failure() for CXL events
* DOE rework

# v6.3+ Future Work
 * Default "Soft Reserved" (EFI_MEMORY_SP) handling policy (cxl-cli + daxctl)
 * Dynamic Capacity Device support
  * Sparse DAX Region infrastructure
  * DCD event plumbing
* Standalone CXL IDE
  * PCIE SPDM pre-requisite
* Maintenance Feature Support (DRAM PPR)
* CXL perf monitoring

# FM Future
* MLD Mailbox support for DCD event injection
* Switch mailbox CCI
  * Multi-head device mailbox tunneling

# QEMU
* Start new threads for debug issues not on patches
* Greg's volatile region setup testing
* Passthrough decoder checks
* SPDM still pending


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
