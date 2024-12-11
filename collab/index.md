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
* Do use Discord as a supplement for this sync meeting for quick questions
* Do follow-up on linux-cxl@vger.kernel.org for longer questions / debug
* https://pmem.io/ndctl/collab/

# December 2024
* Opens
* cxl-cli
* QEMU
* v6.13 rc fixes
* v6.14 merge window

## Opens
<none>

## cxl-cli / user tools
* v81 open: misc fixups and unit test updates
* DCD cxl cli series needs review (Ira)
  - Ira update?
  - alter test to pass with known lockdep issue
* Sanitize memdev functionalities (DavidLorh)
  - Need review
  - sanitize and secure will be mutually exclusive

## QEMU
* in stabalization phase, nothing now
* some fixes for generic ports
* future
  - low hanging fruit - minor features
  - 3,6,12 interleave
  - CHMU feature
    - no real hardware emulation due to IP issues
    - probably better than real hardware


## 6.13 rc fixes
* cxl/pci: Check dport->regs.rcd_pcie_cap availability before accessing
  - Ready to queue
* cxl/region: Fix region creation for greater than x2 switches
  - Ready to queue
* Fix potential bogus return value upon successful probing
  - Davidlohr
  - https://lore.kernel.org/all/20241115170032.108445-1-dave@stgolabs.net/

## 6.14 merge window
* Rest of DCD series (Ira)
  - Cleanup parts landed in 6.13.
  - v7 posted. Pending review from Dan
  - lockdep issue dev_uevent - uevent_show()
* Support background operation abort requests (Davidlohr)
  - v1 review needed
* CXL PCIe port protocol error handling and logging (Terry)
  - v3 posted, review on going?
  - v4 1.1 test issues - defer 1.1 changes? (yes)
    - PCIe core are all 2.0 changes
    - no regressions on 1.1
    - 1.1 could be made to use the same flow as VH (but more time is needed)
    - would be easier for Bjorn to review as well
    - Jonathan nervous about differences between PCIe/CXL iterators
* Type2 device support (Alejandro)
  - v7 posted, need review
  - please concentrate on patches without tags
  - prelim patches for Dave to land first
    - Reviewed by Jonathan
    - https://patchwork.kernel.org/project/cxl/patch/20241203162112.5088-1-alucerop@amd.com/
* Trace FW-First CXL Protocol Errors (Smita)
  - v3 posted, review on going?
* Add device reporting poison handler (Shiyang)
  - Waiting on v5?
* Update soft reserved resource handling (Nathan, Alison)
  - v1 is posted (w RFC feedback) and needs review, esp DAX notifier
  - share notifier between dcd code and soft reserved
    - clean up later if needed
  - Do we need to add checking or handling for DCD?
* Introduce generic EDAC RAS control feature driver (Shiju)
  - v17 posted. Review ongoing with Boris
  - Needs to sync "feature" calls with CXL fwctl
    - please don't block EDAC on fwctl (no)
    - But will sync code paths
* FWCTL CXL (Dave)
  - Create a features driver/device to handle all feature support. Driver/device needed to prevent FWCTL from being loaded by CXL core. Need to coordinate with Shiju. 
  - Drop RFC and pending v1 to be posted upstream  
* Add exclusive caching enumeration and RAS support (Dave)
  - v1 posted, need review.
* Enable Region creation on x86 with Low Mem Hole (Fabio)
  - v1 posted, need review.
  - This is platform specific x86'ism
  - there was at least one other memory hole example
    - do we have that concrete example?
    - will need to get approval for release
* Update event records to CXL spec r3.1 (Shiju)
  - v4 posted, need review
  - tracepoints are now too long for libtracept (> PAGESIZE)
  - Working with Steven to determine correct fix
  - uuid support to be added too
* Rename ACPI_CEDT_CFMWS_RESTRICT_TYPE2/TYPE3 (Ying)
  - Looks like needs a conclusion with the discussion
* Cleanup add_port_attach_ep() "cleanup" confusion (Dan)
  - v2 posted, need review?

## 6.15 and beyond
* Hotness driver
  - basic enablement - tracing driver
  - send to userspace for user to decode
  - in kernel use?
  - convert PA to user app VA?
  - DAEMON? - Jonathan thinks "no"
* vfio-cxl?
  - Any new updates?

## Admin issues
* Should we make this 1.5 hour, 2?
* Share the load of running meeting?


# November 2024
* Opens
* cxl-cli
* QEMU
* v6.13 fixes
* v6.14 merge window

## Opens
  * whats next?  How to support DCD in VMs?  Continue discussion from LPC.


## cxl-cli / user tools
* v81 open: misc fixups and unit test updates, expect release w. kernel 6.13
* DCD cxl cli series
  - need review

## QEMU
* merge window finished next week
  * topology discovery
    * lane details (speed/width)
* payload checks missing which could attach the host
  * fixed.

# Next stuff
* 3/6/12 interleave upstream


## 6.13 fixes
* cxl/region: Fix region creation for greater than x2 switches (Huaisheng)
  - waiting for v2

## 6.13 merge window (in next)
* Constify range_contains() input params
* Add CXL 1.1 device link status support for lspci
* Downgrade warning message to debug in cxl_probe_component_regs()
* Add printf specifier '$pra' for 'struct range'
* Add cleanup/prep code (first 6 patches) from DCD series
* Cleanup add_port_attach_ep() "cleanup" confusion (Dan)
  - v3 soon

## 6.14 and beyond
* Rest of DCD series (Ira)
  - Pending review from Dan
* Support background operation abort requests (Davidlohr)
  - Review needed
* CXL PCIe port protocol error handling and logging (Terry)
  - Review on going?
  - v2 posted -> v3 comming
  - later patches need more looks
  - pci does not look at upstream ports but Terry's patch set did
    - Fix in pci and would make cxl easier? (AER info)
    - Would be nice for the PCI folks to explain the current PCI behavior (ping Lukas?)
  - CXL protocol errors are reported through AER
    - AER driver determines CXL device and passes to CXL for further decode
  - uncorrectable error (fatal or non-fatal) -> panic
    - report info first (pci link still up so should still be able to use link)
    - We don't know who is using it (acclerators [through specific driver] might be able to recover)
    - Don't always panic but...  the details are use case specific
* Type2 device support (Alejandro)
  - Waiting for v5?
    - v5 soon
    - later patches need more looks
    - minor fixes and more testing
* Trace FW-First CXL Protocol Errors (Smita)
  - Waiting for v3? -> yes
  - does call into the AER handler
  - patch sets can't land independent for fatal error case
    - Terry's lands first for Smita to use
* Defer probe when memdev fails to find correct port (Gregory)
  - Waiting for v2?
  - Obsoleted with Dan's patch set
  - Dropping
* Add device reporting poison handler (Shiyang)
  - Discussion still on going?
  - Need to route to ras daemon?
  - ras daemon deals with so many vendor specifics it is fine to let ras daemon deal with it.
* Update soft reserved resource handling (Nathan, Alison)
  - Alison testing has not been reproduced by Nathan
  - Do we need to add checking or handling for DCD?
  - Notifications of the dax driver still being looked into
    - need to notify of soft reserved after region created? 
* Introduce generic EDAC RAS control feature driver (Shiju)
  - Review needed for CXL bits
  - Boris review for sysfs (has not said he likes it yet)
  - memory repair stuff CXL spec a bit of a mess
  - one device per feature kind of clean

## RFCs?
* FWCTL CXL
  - v2 coming. Jason requested posting sooner than later.
* Extended Linear Cache support
  - v2 coming. Looking for ideas on properly detecting MMIO hole in kernel
* vfio-cxl?
  - new summary out
  - planning to send a new patch set based on Alejandros v5
  - white paper?
  - Leverage device ACL registers if possible
    - fall back to vendor driver

## open discussion
* Jonathan ACPI discussion
* Hotplug CXL fixed memory window review -> going through mm tree
  - Mike Rapaport review should be good
  - not critical yet
* DCD set up
  - expose DCD in a VM
    - long ago discussion from Jonathan
  - Find device by Tag (VM and bare metal should work the same)
  - John provide an on-list conversation

# October 2024
* Opens
  - Thanks for the LPC-MC organizers!!!
  - <none>

* cxl-cli
* QEMU
* v6.12 fixes
* v6.13 queue
* v6.13+
* Vfio-cxl


## cxl-cli / user tools
* v80 released and working a build issue w CentOS Stream10
* v81 open: expecting DCD, pmem_ns, and misc unit test patches

## QEMU
* 3 sets out for next
  - generic ports
  - speed control
  - fixes
* 3/6/12 interleave outstanding
* MCTP payload sizes might get sliped in
* NVME changes
  - blocker was tests


## 6.12 fixes
* v6.12 PR completed
* Fix CXL device SBDF calculation (in RC3)
  - https://lore.kernel.org/all/6701760b2e390_16041829420@iweiny-mobl.notmuch/
* Fix Trace DRAM Event Record (
  - https://lore.kernel.org/linux-cxl/05305df495904b9f99fcb52f67a66762@huawei.com/T/#t
* Fix CXL port initialization order when the subsystem is built-in
  - https://lore.kernel.org/linux-cxl/0c945d60-de62-06a5-c636-1cec3b5f516c@amd.com/T/#t
  - [Johnathan: Comments in the makefile]
  - spelling fix -- will spin for now
  - Ack from GregKH would be nice
* Fix KASAN error in cxl-test
  - 
 
* Other trees
* Poll DOE Busy bit for up to 1 second in pci_doe_send_req
  - https://lore.kernel.org/linux-cxl/20241013155834.GA607803@bhelgaas/T/#mfe07920c273fe28eb9acce1b2bb509a69860e6de
* Support missing events in 3.1 spec - 6.13
  - https://lore.kernel.org/linux-cxl/20241014141551.GA17702@willie-the-truck/T/#t


## 6.13 queue
* EDAC scrub
  - https://lore.kernel.org/linux-cxl/20241009124120.1124-1-shiju.jose@huawei.com/T/#m6f07b22ae7a00dd7ecf0095cff2683b88aa21ea7
* Enable CXL PCIe port protocol error handling and logging
  - https://lore.kernel.org/linux-cxl/d9d87f72-6273-4adc-934c-e25ee79bb8c7@amd.com/T/#mc0de8ae4187b55227201bae525596d49cad196ba
* Smita - CPER changes
* DCD: Add support for Dynamic Capacity Devices (DCD)
  - v4 is out, v5 soon
  - https://lore.kernel.org/linux-cxl/6707f33c89730_4042929481@iweiny-mobl.notmuch/T/#m6267352b7ea3215257311a05cb058aaf7e12b522

## 6.13 order?  -- What should we focus on?
1 DCD - dax review - if this is fine.
  - remove printk stuff
2 Port protocol error handling...
  - Need community agreement
3 Smita's patches
4 RFC for soft reserved - dax again...
4 (parallel) EDAC scrub not a lot more cxl review needed


## 6.13+ queue
* Type 2 patch sets
  - (Alejandro) https://lore.kernel.org/linux-cxl/20240907081836.5801-1-alejandro.lucero-palau@amd.com/T/#mf3427aeb28f1eecc5df167cfd15af22aaec4a1d5
  - (Ying) https://lore.kernel.org/linux-cxl/20240925024647.46735-1-ying.huang@intel.com/T/#mb2484ab566353dbf58bd905d7affb1549bb15fad
* VFIO-type 2
  - https://lore.kernel.org/linux-cxl/4230fba5-030c-49ef-799e-f4138b1c9f7d@amd.com/T/#mcac344899aacd07f98c4132d25d2ad29a6da9b09
  - https://lore.kernel.org/linux-cxl/20240921071440.1915876-1-zhiw@nvidia.com/T/#m7e6f2b0ac88102095120a56d55341477b74a737a
  - Dan: Trend is bare-metal sets up resources and hands them to the guest
  - PCI BAR mapping does not change for the guest so why would this be different?
  - Also have to consider the CXL.cache case for type-2.  This is more demanding.
  - What kind of comunication between guest driver and VMM for CXL.cache?
  - Alejandro working on a doc to share with the mailing list.
  - Look at resizable BARs in VFIO.

## Init order talks
Nothing which stops an accelerator to skip the cxl_mem driver.  And there are reasons not to.

## ARM stuff
non-x86 how do we do cache flush (invalidate all)
Every ARM implementation seems to do this differently
Will need more infrastructure -- a subsystem to cache flush
PRM spec?

Dan's unicorn wish is for CXL spec to add this...


# September 2024
Skipped due to LPC


# August 2024
* Opens

* QEMU
* v6.11 fixes
* v6.12 queue
* v6.12+

## QEMU

## cxl-cli / user tools

## 6.11 fixes
* cxl/region: Remove lock from memory notifier callback
  - Maybe destined for 6.12 merge window?
* Fixes for hdm decoder initialization from DVSEC ranges
  - Need review tags

## 6.12
* cxl: Region bandwidth calculation for targets with shared upstream link
  - Waiting on Dan's further review
* cxl/port: Use _-free() to drop put_device() for cxl_port
  - Need reviews
* cxl: add Type2 device support
  - Pending v3 from Alejandro, discussion on going
* Address translation for HDM decoding
  - Pending v3 from Robert
* cxl/region: Remove soft reserve resource at region construction
* acpi/ghes, cper, cxl: Trace FW-First CXL Protocol Errors
  - Waiting on next rev from Smita. No activity since May. Is this still happening?  
* DCD: Add support for Dynamic Capacity Devices (DCD)
  - v3 is out, please review

# 6.12+
* Add RAS support for CXL root ports, CXL downstream switch ports, and CXL upstream switch ports
  * Terry, new updates?
  * Fan is testing
* Scrub Subsystem via EDAC
  * v11 posted
  * Shiju/Jonathan, new updates?
* Extended-linear memory-side-cache HMAT Proposal
  * Approved?
  * ACPICA pull request merged for new HMAT definition
  * WIP
* CXL Error Isolation
  * awaiting a user, paused
* CXL PMU support for switches
  * Jonathan, updates?
* cxl: avoid duplicating report from MCE & device
  - Nothing new since last meeting
* FWCTL CXL support
  - Awaiting v2 and address Jason and Jonathan's comments


# July 2024
* Opens
  * invitation for tech topics for Plumbers uConf spillover
  * device-passthrough followup
    * vm capability to find memory by tag?
    * passthrough DCD (existing CXL emulation) vs passthrough DAX region (new emulation)
    * are there nested use cases for passing through pooled memory?
    * pooled memory for VMs pre-DCD is not widespread, but chicken / egg problem to become a prom
* QEMU
* cxl-cli
* v6.11 fixes
* v6.12 queue
* v6.12+

## QEMU
* DCD landed since last meeting (basic infrastructure for kernel enabling)
* MST pull request for (sanitize, scan media, get feature (scrub/ecs), libspdm NVME target)
  * generic port support will circle back
  * PCI CMA not using libspdm found a libspdm bug as a result
* MCTP support?
  * only aspeed i2c controller support it and awkward to support on x86 (ACPI)
  * PCI -> DT -> I2C?
  * Request for a PCI VDM MCTP that is easier to emulate? Inventing one probably too much effort
  * virtio-i2c not mctp capable

## cxl-cli / user tools
* [ndctl PATCH v13 0/8] Support poison list retrieval
  - [Patches 1 & 3 need tags](https://patchwork.kernel.org/project/cxl/list/?series=868958)
* ndctl v80 waits for poison list
  * coincident with v6.11

## 6.11 fixes
* None, currently in 6.11 merge window. Will do PR tomorrow (Wednesday)

## 6.12
* cxl: add poison creation event handler
  - Waiting on comments from Dan?
* cxl: Region bandwidth calculation for targets with shared upstream link
  - Waiting on further review from Dan.
* cxl: add Type2 device support
  - Going through reviews
* Address translation for HDM decoding
* Waiting on next revision from author:
  * Export cxl1.1 device link status register value to pci device sysfs.
  * cxl/region: Remove soft reserve resource at region construction
  * acpi/ghes, cper, cxl: Trace FW-First CXL Protocol Errors

# 6.12+
* DCD: Add support for Dynamic Capacity Devices (DCD)
  * Ira: Reordering patches, asking for new tags
    * 'more bit' and multiple regions per partition included
* Add RAS support for CXL root ports, CXL downstream switch ports, and CXL upstream switch ports
  * Terry: Incorporating RFC feedback to follow RCH downstream port flow
* Scrub Subsystem review
  * Shiju / Jonathan: bringing it inline with EDAC requirements from Borislav
    * ECS and PPR to be included
* Extended-linear memory-side-cache HMAT Proposal
  * Dan: on track for approval
* CXL Error Isolation
  * awaiting a user
* CXL PMU support for switches
  * Jonathan: stalled behind pcieport rework
    * Bind problem for switches relative to class code (pci conventional bridge code)
* cxl: avoid duplicating report from MCE & device
  * Shiyang: are duplicated reports a problem in practice? Maybe when "repair" becomes more widespread
  - Going through reviews
* FWCTL CXL support
  * RFC posted, Jason says looks ok, need CXL review on policies
* Mailbox refactoring
  * make cxl_dev_state public? wait to see use cases
* Spreading CFMWS x86 policy to other archs: numa_memblk for more archs

# June 2024
* Opens
* QEMU
* cxl-cli
* v6.10 fixes
* v6.11 queue
* v6.11+

## QEMU
* Big endian conflicts for generic port... fix triggered signficant review feedback
* Waiting for some merge confusion to resolve before pushing more
* Next up
  - Misc fixes (no multiple memory backends)
  - Scan media
  - Sanitize fixes
  - 3,6,12,16 way interleave support
  - 'Get Feature' support
  - Introduce comprehensive variability for generic port for test coverage (bottlenecks in multiple places)
  - Investigate fine grained CDAT configurability
* MCTP backed up on an NVME dev branch
* DMA Bounce Buffering pending on separate tree
* FW Update: v3 pending clarification feedback
* FW First Error injection support, is now a generic capability
  - ARM-only at the moment
  - x86 review / help needed with emulating SCI mechanics to signal errors
* DCD should land this cycle

## cxl-cli / user tools
* cxl/test: add cxl_translate unit test
  - [New patch](http://lore.kernel.org/20240624210644.495563-1-alison.schofield@intel.com), needs review: 
* cxl/test: Add test case for region info to cxl-events.sh
  - [Needs additional review](http://lore.kernel.org/20240328043727.2186722-1-alison.schofield@intel.com)

## 6.10 fixes
* cxl/mem: Fix no cxl_nvd during pmem region auto-assembling
  - locally queued
* cxl/region: Avoid null pointer dereference in region lookup
  - Ready to be queued, review still welcome
* Check interleave capability
  - ready to be queued
* XOR Math Fixups: translation & position
  - Need review tags, top priority for fixes pull
  - Looking to merge for v6.10-rc6

## 6.11 merge window
* cxl: add missing MODULE_DESCRIPTION() macros
  - locally queued
* cxl/region: Support to calculate memory tier abstract distance
  - locally queued, review still welcome

## Need additional reviewing and tags
* cxl: add poison creation event handler
* acpi/ghes, cper, cxl: Trace FW-First CXL Protocol Errors
* Documentation: CXL Maturity Map
  - awaiting v2 posting
* Export cxl1.1 device link status register value to pci device sysfs.
  - Have tags from Jonathan. Dan can you take a look?
  - lspci already has support on the pending ABI
* cxl: Region bandwidth calculation for targets with shared upstream link
  - Have tags from Jonathan. Pending test tag from Jonathan. Some additional minor updates. Can use additional review.

## Waiting on next revision from author
* cxl/acpi: Warn on unsupported platform config detection
* cxl/region: Remove soft reserve resource at region construction

# 6.11+
* DCD: Add support for Dynamic Capacity Devices (DCD)
  - Cleaning up lifetime of extent devices
  - 'More' bit support in progress
  - ndctl enhancements (extent listing suppport)
  - Misc cleanups
* Add RAS support for CXL root ports, CXL downstream switch ports, and CXL upstream switch ports
  - Option 2 consideration
* Scrub Subsystem review
  * Shiju
  - Any new updates since last meeting?
  - new version pending, all under EDAC, but modernized to be a 'struct bus_type'
  - Need per-device *and* per-region scrubbing
  - CXL regions and memdevs registered on the EDAC bus
  - DDR ECC6 does have per-dimm controls, so *maybe* there is a DDR-DPA concept
  - Respond to high correctable events with increasing the scrub rate? (POC)
  - DPAs are not physical? ...but system-software pretends they're physical
* Extended-linear memory-side-cache HMAT Proposal
  * Dan
* CXL Error Isolation
  * awaiting a user
* CXL PMU support for switches
  * Jonathan
* cxl: avoid duplicating report from MCE & device
  * Shiyang
  - Going through reviews
* FWCTL CXL support
  * DaveJ working on RFC, get/set features support


# May 2024
* Opens
  * LSF/MM takeaways
    * famfs status
      * FUSE maintainers are interested in supporting famfs
      * Passthrough for inodes and faults working
      * famfs needs some support on top, iomap support still needed
      * memfd support?
    * filesystem in Rust discussion
    * weighted interleave (now fully upstream!) takeaways
      * container developers are a bit more tolerant of details
      * cluster owners want to know a bit more
      * folks open to improvement to numactl
      * Jonathan: get in touch with Brice about hwloc  updates?
    * Peter Xu hugetlb fixups
  * LPC: uConf accepted, get submissions in and register with CXL uConf as planned attendee
    * Vienna Sept 16th
    * CFP deadline early July
  * CXL Maturity Map

* QEMU
* cxl-cli
* v6.10 fixes
* v6.11 queue
* v6.11+

## QEMU
* Fan posted new version of QEMU-DCD support
* Generic port support got review and prompted docs updates
* MHD support making way for DCD support to clear first

## cxl-cli / user tools
* v79 release since last meeting, and v80 open

## 6.10
* cxl/region: Fix memregion leaks in devm_cxl_add_region()
  * Ready for queue
* [/4] XOR Math Fixups: translation & position
* [/2] cxl/region: Fix potential invalid pointer dereference
* cxl: Add interleave capability check
* cxl/pci: Convert PCIBIOS_* return codes to errnos

## 6.11 queue
* cxl/events: Use a common struct for DRAM and General Media events
  * Ready for queue
* cxl/region: Convert cxl_pmem_region_alloc to scope-based resource management
  * Ready for queue
* [/2] cxl: add poison creation event handler
* [/3] Display cxl1.1 device link status
* [/26] DCD: Add support for Dynamic Capacity Devices (DCD)
  * Wait for next rev

## 6.11+
* Scrub Subsystem review
  * ACPI RAS2 user showed up on the list
  * Converging on "new EDAC" today only "mc" and "pci" devices, switch to properly parented devices
  * class device to associated memory regions to scrub interfaces
  * why does userpace want to control this and how
  * would there be an auto-scrub control in userspace? e.g. Increase scrubbing on threshold crossed
* Extended-linear memory-side-cache HMAT Proposal
* CXL Error Isolation
  * awaiting a user
* Switch Port Error handling pending
* CXL PMU support for switches
  * Refactored portdrv to aux-bus RFC to be posted soon


# April 2024 (2)
* Opens
  * FAMFS RFC v2 posted
* QEMU
* cxl-cli
* v6.9 Fixes
* v6.10 queue
* v6.10+

## QEMU
* DCD Feedback for 9.1 dev cycle
  * last mile feedback but looking positive for this cycle
* Generic Port feedback also started
* Reset handling Fixups
  * For AER flow testing
  * SBR masking emulation testing
  * Useful for kexec testing
  * CXL Reset digression
* Interleave testing
  * Commit error checking and interleave functionality
  * precursor - DMA bounce for TCG, MCTP still pending (NVMe-MI dependency)
* MHD Support for Orchestrator flow testing
* SPDM is a precursor for QEMU tree backlog

## cxl-cli / user tools
* Media error patch pending awaiting final review
* Minor fixups
* rasdaemon has CXL patches pending to log events to DB
* OCP is working on expanding CPER record types for hyperscale RAS use cases

## v6.9

* [0/2] cxl: add interleave capability check
  * Waiting for next rev, likely 6.10 material
* cxl: Fix cxl_endpoint_get_perf_coordinate() support for RCH
  * Pending for 6.9-rc7, Thanks Robert!

## v6.10 Queue

* cxl: Calculate region bandwidth of targets with shared upstream link
  * Need review
* [0/4] PCI: Add Secondary Bus Reset (SBR) support for CXL
  * Pending review from Dan and Bjorn
* cxl/hdm: Debug, use decoder name function
  * Queued.
* [0/4] efi/cxl-cper: Report CXL CPER events through tracing
  * Waiting for Rafael review tag. Probably for 6.11
  * Ping Tony to take a look
* [0/3] Display cxl1.1 device link status
  * Waiting for next rev and review
* [0/2] Add log related mailbox commands
  * Pending queueing
* [0/4] Add DPA->HPA translation to dram & general_media
  * Wait for next rev
  * likely 6.10 material
* cxl/test: Enhance event testing
  * Queued
* cxl/hdm: Add debug message for invalid interleave granularity
  * Queued
* cxl/acpi: Cleanup __cxl_parse_cfmws()
  * Pending next rev
* cxl/acpi.c: Add buggy BIOS hint for CXL ACPI lookup failure
  * BIOS fix provided, no need to add this hint
* MAINTAINERS: repair file entry in COMPUTE EXPRESS LINK
  * Pending next rev
* cxl/cxl-event: include missing <linux/types.h> and <linux/uuid.h>
  * Queued
* [0/2] cxl: add poison creation event handler
  * Pending next rev
* cxl: Fix use of phys_to_target_node() for x86
  * Queued

## v6.10+
* [00/26] DCD: Add support for Dynamic Capacity Devices (DCD)
  * Wait for next rev
* Scrub Subsystem review
  * ACPI RAS2 user showed up on the list
* CXL Error Isolation
  * awaiting a user
* XOR translation
* Switch Port Error handling pending
* CXL PMU support for switches

# April 2024 (1)
* Opens
  * CXL Maintainer Update
* QEMU
  * QEMU is in the quiet period for next 2-3 weeks
  * TCG memory update, do not use virtio-storage that might put a DMA buffer in CXL memory
  * Generic port in staging, cleaned up version to be posted soon
  * DCD Update
    * DVSEC Ranges? What to do if no static capacity: cxl_await_media_ready() (Linux currently needs active + valid at a minimum)
    * Might need a spec clarification for pure DCD devices
    * v6 pending review
    * Release of pending extents => no force removal flow in QEMU for now
* cxl-cli
  * v79 awaiting final review on poison listing changes
  * Alison to be maintainer / patch wrangler post v79 release
  * libcxl-mi modeled on NVME-mi? MCTP investigation
  * BMC Switch CCI + MCTP unification, but maybe not inband
* v6.9 Fixes
* v6.10 Queue
* Future

# March 2024
* Opens
  * FAMFS update
* QEMU
* cxl-cli
* v6.8 Fixes
* v6.9 Queue
* Future

## QEMU
* 8 week merge cycle still open
* Pre-reqs pending
  * SPDM from Alistair
  * MCTP
  * Bounce buffer fix for DMA to CXL memory
  * Generic port pending generic initiators from NVIDIA
* DCD Emulation Update
  * Feedback incorporated
  * Superset extend/release as well partial extend/release supported
  * Tests passing
  * Formalizing introspection may make sense in the future
  * Might be too late for cycle ending in a week or so
  * Greg: Multi-head interactions with DCD emulation? Can it be added incrementally?
    * Investigate a shared base device for single/muti-head implementations to share
    * Greg to RFC what he has
* Generic Port
* Background command support queued behind DCD for convenience
* MCTP over I2C still in process
* ARM support still pending device-tree interactions, help welcome
* Firmware first error handling, not impossible to upstream but not a priority
* CPMU
* FM-API help to flesh out the command support welcome
* Non-interleaved high performance CXL memory emulation (how to represent the performance)
* QEMU only emits x1 lowest bandwidth link speed

## FAMFS
* Review, thanks Jonathan
* DEVICE-DAX IOMAP review needed
* PMEM support may be dropped in favor of just DAX
* Christian Brauner has advice on how to open dev-dax
* Fault counters to be removed
* FAMFS held up to performance benchmarking
* Superblock identified capacity
* Initial use case: provide access to a large shared pool with readonly clients

## cxl-cli
* [List Media Errors (Poison)](http://lore.kernel.org/r/cover.1705534719.git.alison.schofield@intel.com): pending review
* QoS class changes; pending
* v79

## v6.8 Fixes
* 3, 6, 12 XOR interleave math fix: pending feedback
* [SSBLIS Fix](http://lore.kernel.org/r/20240301210948.1298075-1-dave.jiang@intel.com): ready to queue
* [CXL QOS Sysfs fixes / simplification](https://patchwork.kernel.org/project/cxl/list/?series=823300): merged
* [Fix "HPA out of order" region assembly fix](https://patchwork.kernel.org/project/cxl/list/?series=821883): merged
* [Fix "no NUMA configuration found"](https://lore.kernel.org/r/99dcb3ae87e04995e9f293f6158dc8fa0749a487.1705085543.git.alison.schofield@intel.com): merged
* [Crash on repeated AER signaling](https://lore.kernel.org/r/20240129131856.2458980-1-ming4.li@intel.com): merged
* [cxl_test build fix](https://lore.kernel.org/r/170543983780.460832.10920261849128601697.stgit@dwillia2-xfh.jf.intel.com): merged
* [Stop requiring MSI/MSIx](https://lore.kernel.org/r/20240117-dont-fail-irq-v2-1-f33f26b0e365@intel.com): merged
* [Fix x16 Region HPA allocation](http://lore.kernel.org/r/20240124091527.8469-1-caoqq@fujitsu.com): merged
* [Fix duplicate messages in CPER handling](http://lore.kernel.org/r/20240131-cxl-cper-fixups-v1-0-335c85b1d77b@intel.com): merged

## v6.9 Queue
* [CXL QOS to NUMA](http://lore.kernel.org/r/170568485801.1008395.12244787918793980621.stgit@djiang5-mobl3): pending merge
* [Weighted Interleave](https://lore.kernel.org/all/20240202170238.90004-5-gregory.price@memverge.com/): queued in mm-unstable
* [DAX support on modern ARM](http://lore.kernel.org/r/20240202210019.88022-1-mathieu.desnoyers@efficios.com): pending merge
* [CXL CPER Protocol Errors to Trace Events](http://lore.kernel.org/r/20240109034755.100555-1-Smita.KoralahalliChannabasappa@amd.com): pending review
* [CXL EINJ](https://lore.kernel.org/all/20240115172007.309547-1-Benjamin.Cheatham@amd.com/): pending merge check ACPICA
* [CXL Userspace Unit Tests](http://lore.kernel.org/r/170171841563.162223.2230646078958595847.stgit@ubuntu): pending review
* [CDAT Cleanups](https://lore.kernel.org/all/20240108114833.241710-1-rrichter@amd.com/): queued
* [CXL test save/restore](http://lore.kernel.org/r/65a980249f50f_3b8e294a3@dwillia2-xfh.jf.intel.com.notmuch): pending non-RFC posting
* [Use sysfs_emit(): throughout](https://lore.kernel.org/r/20240112062709.2490947-1-ruansy.fnst@fujitsu.com): queued
* [cond_guard(): and related cleanups](http://lore.kernel.org/r/20240205142613.23914-1-fabio.maria.de.francesco@linux.intel.com): pending next posting
  * scoped_cond_guard() usages pending for v6.9

## Future
* [Component State Dump](https://lore.kernel.org/linux-mm/20240222172350.512-2-sthanneeru.opensrc@micron.com/T/) interaction with event clearing 
  * how much data is in a CSD, how much blob can trace event support
* [CXL Scrub Feature](http://lore.kernel.org/r/20240111131741.1356-1-shiju.jose@huawei.com): more review needed
  * DRAM Scrub necessary over time
  * Tradeoffs of reliability vs scrub cost
  * want hotplug support
  * Address Range Scrub, on demand scrub
  * new patchset in process
  * sync with RAS API folks on reusability
  * OpenCompute model of out of band control might be in conflict with embedded use cases
  * RAS API does not supply stop-scrub on inband interface
* CXL Switch Port Error Handling: pending initial posting
* CXL Root Port (RCEC Notified): Error Handling: pending initial posting
* DCD: pending next revision
* DPA to HPA translation for events
* Type-2 Preview: still awaiting a consumer
* CCI Refactor for Switch CCI, RAS API, Type-2: pending next posting
* MMPT in Jonathan's queue



# February 2024
* Opens
  * [LSF/MM CFP](https://lore.kernel.org/bpf/4343d07b-b1b2-d43b-c201-a48e89145e5c@iogearbox.net/): deadline March 1st
* QEMU
* cxl-cli
* v6.8 Fixes
* v6.9 Queue
* Future

## QEMU

* Status
  * 2 patch sets to pick up; bunch of fixes
  * Not clear spec versions so update those to 3.1

* Fan's next DCD version; close
  * Some minor issues
  * Would like to land 9.0 cycle (Aprox end of March)
  * Some things depend on these so want to land them first

* MHD won't make March

* TCG/KVM mess
  * Bug report on list; Not as minor as thought
  * Slow path does not cover everything unfortunately
  * May be some other issues
  * Random crashes (might be page tables or ??)
  * Alternative is to implement performance path
   * Treat as normal RAM
   * Can't do interleave with lots of memory regions (ways?)
* For now... Don't use emulated CXL memory
* Fan said it would work for some cases?
  * Kernel code is now putting things in the right numa nodes
  * Kernel may have been using swap
* Should x86 use memblock?
  * Jonathan does not think it will help
* Re-read cdat?
* EFI soft reserved causes x86 to keep the info around
  * 'numa keep meminfo' or something like that

* AMD CPER pushed out
  * Jonathan would like a HEST table from x86 if someone could provide that



## cxl-cli
* [List Media Errors (Poison)](http://lore.kernel.org/r/cover.1705534719.git.alison.schofield@intel.com): pending review
* QoS class changes; pending
* Porcelain patches welcome
  * How can we make things easier?
  * Automate cxl create region for largest regions it can figure out


## Should Linux be the BMC?
* Open BMC has a lot of drivers
* need guard rails
* Might be useful and to share code
* BMC only use cases are questionable
  * How do we ID which is which?
  * Kconfig CXL_BMC_SUPPORT?
  * Similart to raw command support


## v6.8 Fixes
* [CXL QOS Sysfs fixes / simplification](https://patchwork.kernel.org/project/cxl/list/?series=823300): pending next posting
* [Fix "HPA out of order" region assembly fix](https://patchwork.kernel.org/project/cxl/list/?series=821883): ready to queue
* [Fix "no NUMA configuration found"](https://lore.kernel.org/r/99dcb3ae87e04995e9f293f6158dc8fa0749a487.1705085543.git.alison.schofield@intel.com): queued for v6.8-rc4
* [Crash on repeated AER signaling](https://lore.kernel.org/r/20240129131856.2458980-1-ming4.li@intel.com): queued for v6.8-rc4
* [cxl_test build fix](https://lore.kernel.org/r/170543983780.460832.10920261849128601697.stgit@dwillia2-xfh.jf.intel.com): merged v6.8-rc2
* [Stop requiring MSI/MSIx](https://lore.kernel.org/r/20240117-dont-fail-irq-v2-1-f33f26b0e365@intel.com): merged v6.8-rc2
* [Fix x16 Region HPA allocation](http://lore.kernel.org/r/20240124091527.8469-1-caoqq@fujitsu.com): merged v6.8-rc2
* [Fix sleeping lock in CPER handling](http://lore.kernel.org/r/20240202-cxl-cper-smatch-v1-1-7a4103c7f5a0@intel.com): pending next posting
* [Fix duplicate messages in CPER handling](http://lore.kernel.org/r/20240131-cxl-cper-fixups-v1-0-335c85b1d77b@intel.com): Going through EFI tree


## AER fatal panic - wide range of handleing
* Policy change to discuss with comunity
  * Instead of hoping we should panic?
   * But if DAX just kill the process (invalidate mappings)
   * But how much running around should we do?
   * Hope was that force remove of driver would do a pr_warn() [let panic on warn crash]
  * Need more real world feedback

## v6.9 Queue
* [CXL QOS to NUMA](http://lore.kernel.org/r/170568485801.1008395.12244787918793980621.stgit@djiang5-mobl3): pending review
* [Weighted Interleave](https://lore.kernel.org/all/20240202170238.90004-5-gregory.price@memverge.com/): queued in mm-unstable
* [DAX support on modern ARM](http://lore.kernel.org/r/20240202210019.88022-1-mathieu.desnoyers@efficios.com): pending final review
* [CXL CPER Protocol Errors to Trace Events](http://lore.kernel.org/r/20240109034755.100555-1-Smita.KoralahalliChannabasappa@amd.com): pending review
* [CXL EINJ](https://lore.kernel.org/all/20240115172007.309547-1-Benjamin.Cheatham@amd.com/): pending resolution of ACPICA dependency
* [CXL Userspace Unit Tests](http://lore.kernel.org/r/170171841563.162223.2230646078958595847.stgit@ubuntu): pending next posting
* [CDAT Cleanups](https://lore.kernel.org/all/20240108114833.241710-1-rrichter@amd.com/): queued
* [CXL test save/restore](http://lore.kernel.org/r/65a980249f50f_3b8e294a3@dwillia2-xfh.jf.intel.com.notmuch): pending non-RFC posting
* [Use sysfs_emit(): throughout](https://lore.kernel.org/r/20240112062709.2490947-1-ruansy.fnst@fujitsu.com): queued
* [cond_guard(): and related cleansups](http://lore.kernel.org/r/20240205142613.23914-1-fabio.maria.de.francesco@linux.intel.com): pending next posting

## Future
* [CXL Scrub Feature](http://lore.kernel.org/r/20240111131741.1356-1-shiju.jose@huawei.com): more review needed
* CXL Switch Port Error Handling: pending initial posting
* CXL Root Port (RCEC Notified): Error Handling: pending initial posting
* DCD: pending next revision
* DPA to HPA translation for events
* Type-2 Preview: still awaiting a consumer
* CCI Refactor for Switch CCI, RAS API, Type-2: pending next posting

# November/December 2023
* Opens
* Plumbers Takeaways
* QEMU
* cxl-cli
* v6.7 Fixes
* v6.8 Queue

## Opens
* Interleave ratios: MVP
  * mempolicy based to start
  * cgroups deferred for a later fight

## Plumbers Takeaways
  * Greg's interleave document
    * 5 types: BIOS, OS, mempolicy (homogenous or heterogeneous)
    * LWN Article for reach? Follow in the style of Mel's NUMA article
  * UKunit: Userspace unit testing of kernel code
    * limitation on what can be mocked with Kunit
    * https://github.com/jimharris/ukunit
  * Davidlohr to post notes
  * Port device RAS support
    * Move PCIe port bus driver logic into PCIe driver/core to start as library
    * AER handler callback to the endpoint driver
    * Break the pcie portbus driver dependency
  * Hotplug range register problem resolution

## QEMU
  * mst picked up more than expected includng CCI support into 8.2-rc1
  * ira's cdat fixes posted
  * scrub control: both QEMU and kernel patches posted
    * Integrate with ACPI scrub control as a subsystem shared with CXL
  * Alistair's SPDM work progression

## cxl-cli
  * concern for first-time users
    * dnf install cxl-cli
    * cxl list -RX
* v79 release imminent
  * corresponding to v6.7 updates
* [hotplug range register support?](https://lore.kernel.org/linux-cxl/ZCRhhUDcmypVKu0X@memverge.com/]
  * disable device mem_enable modify range register + re-enable
  * how to handle zero based DVSEC range register

## v6.7-fixes
* locking fixups

## v6.8
* Interleave syscall
  * John: don't force people to go through BIOS for interleave
  * Michal: looking for mempolicy2() support
  * Greg: also working on thrid-party mempolicy syscall via pidfd (minus mbind/homenode)
    * once syscalls are in interleave weights can be layered on top without ABI changes
    * numactl changes would be nice to have 

# "Halloween" 2023
* Opens:
* QEMU
* cxl-cli

## QEMU
* Multiple HDM decoder support landed
* Compilation issues slowed down a topic
* Mailbox CCI rework sent out
  * Difficult to test MCTP infrastructure
* Fan in process of next DCD posting
* FMAPI support on top of DCD ("add" support, test interfaces included "real" tooling wanted)
* QEMU support for changing QOS class information?
  * weighted interleave investigation
  * generic target support needed

## cxl-cli
* sanitize command unit test for (for v80 depends on v6.7)
* poison listing support (for v79 kernel support in v6.5)
* automatic region position determination for create-region (--strict option for recovery of old behavior)

## v6.7

## v6.8
* DCD next revision pending
* Spec pipecleaning in progress
* [Node Weights and Weighted Interleave - Gregory Price](https://lore.kernel.org/linux-mm/20231031003810.4532-1-gregory.price@memverge.com/)
* John: Tier preference vs local preference?
  * Gregory: bandwidth vs latency tiering conflicts

# October 2023
* Opens:
  * Jim: QEMU dport conflicting connections, (1) HB (1) 1 RP (2?) Switches (4) Endpoints (Who detects impossible configs?)
  * Gregory: port to region confusion (make create-region smarter)
  * Vincent: multi-function upstream ports? Yes, for PCIE, does CXL mandate function0?
  * Steve: RCH link width / speed enumeration (emit via CXL objects?) Jonathan RCIEP examples of emitting attributes, virtual switch?
  * Jonathan: Dynamic NUMA node creation
    * 0-size NUMA node entries in SRAT already shipping
* QEMU
* cxl-cli
* v6.6 Fixes
* v6.7 Queue

## QEMU
* Cleanup sets upstream
* mst has QTG in the backlog
  * backlog of PCI bits
  * switch serial number on upstream port
  * multi-HDM decoders
  * mailbox rework for Switch CCI + MCTP over I2C (difficult to add aspeed to x86 machine model)
* DCD: working through reported issues wrt kernel patches
* Fabric management ambiguities
  * MCTP representation of MLDs? Single-MLDs when plugged in as an SLD.
  * FMAPI binding when sending to a switch, not Type-3, except for the general commands like identify
  * I.e. use type-3 binding except opcodes 0x4000+ when talking to a switch

## cxl-cli
* [Poison List Retrieval](https://patchwork.kernel.org/project/cxl/cover/cover.1696196382.git.alison.schofield@intel.com/)
* [Towards CXL continuous integration](https://github.com/facebookincubator/kernel-patches-daemon)
* Vishal: [set alert config patches](https://lore.kernel.org/linux-cxl/20230918045514.6709-1-jehoon.park@samsung.com/) queued up

## v6.6 fixes
* [v6.6-rc3 update](https://lore.kernel.org/linux-cxl/650f60a224347_124e92943@dwillia2-mobl3.amr.corp.intel.com.notmuch/)
* [Fix shutdown order](https://patchwork.kernel.org/project/cxl/cover/169602896768.904193.11292185494339980455.stgit@dwillia2-xfh.jf.intel.com/)
  * awaiting testing
  * need to rework mbox irq to be threaded or an atomic flag
* [Soft Reserved Conflict / Lifetime](https://patchwork.kernel.org/project/cxl/cover/cover.1692638817.git.alison.schofield@intel.com/)
* Auto-assembly Rework
  * Jim: Granularity fix top down is confusing switch settings
* Davidlohr: Type-2 crash interaction with security shutdown order?

## v6.7+
* [RCH EH](https://patchwork.kernel.org/project/cxl/cover/20230927154339.1600738-1-rrichter@amd.com/)
* [QTG](https://patchwork.kernel.org/project/cxl/cover/168695160531.3031571.4875512229068707023.stgit@djiang5-mobl3/)
* [QTG to HMEM](https://patchwork.kernel.org/project/cxl/list/?series=759643)
* [Switch CCI](https://patchwork.kernel.org/project/cxl/cover/20230804115414.14391-1-Jonathan.Cameron@huawei.com/)
  * Davidlohr: background status publishing to userspace? Bind VPB, Sanitize via Tunnel?
  * Jonathan: Punt until someone with BMC background can help drive
  * Jonathan: Possibly some NVME MCTP work to draft behind
  * Jonathan: start with safe commands to get framework started
  * Gregory: multi-headed SLD testing validating the approach of an independent mailbox core (QEMU)
* [SPDM / Auth](https://patchwork.kernel.org/project/cxl/cover/cover.1695921656.git.lukas@wunner.de/)
  * SPDM BoF Planned for Plumbers in November
* [memmap on memory](https://patchwork.kernel.org/project/cxl/cover/20230928-vv-kmem_memmap-v4-0-6ff73fec519a@intel.com/)
* mempolicy proposals:
  * [multi-tier](https://patchwork.kernel.org/project/cxl/cover/20230927095002.10245-1-ravis.opensrc@micron.com/)
  * [mempolicy2](https://patchwork.kernel.org/project/cxl/cover/20231003002156.740595-1-gregory.price@memverge.com/)
  * [mempolicyNM](https://patchwork.kernel.org/project/linux-mm/patch/20220607171949.85796-1-hannes@cmpxchg.org/)
  * [weighted interleave]
  * Informal Plumbers BoF


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
