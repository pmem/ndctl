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

# START THE TRANSCRIPT

# Aug 18, 2026
## Agenda
* Opens
* cxl-cli
* QEMU
* v7.3 rc fixes
* v7.4 merge window
* v7.5 and beyond

## Opens
* A few words about Dan Williams

## CXL CLI

## QEMU

## v7.3 rc fixes
* None. v7.3 PR sent 8/17

## v7.4 merge window
### Fixes
- **Harden HDM decoder enumeration (v5)**
  - Alison Schofield <alison.schofield@intel.com>
  - https://lore.kernel.org/linux-cxl/cover.1786143520.git.alison.schofield@intel.com/
  - Mostly ready. Need review tags.

- **cxl: Sashiko bug fixes (v5)**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260813034538.13189-1-icheng@nvidia.com/
  - Mostly ready. Need review tags.

- **cxl: Allow passthrough decoders with >16K granularity (v3)**
  - Alison Schofield <alison.schofield@intel.com>
  - https://lore.kernel.org/linux-cxl/cover.1784940306.git.alison.schofield@intel.com/
  - Mostly ready. Need review tags.
    
- **cxl: Fix uninitialized access coordinates (v3)**
  - Guixin Liu <kanie@linux.alibaba.com>
  - https://lore.kernel.org/linux-cxl/20260812083035.372308-1-kanie@linux.alibaba.com/
  - Needs review
    
- **cxl/core: Fix dport use-after-free via the einj_inject debugfs file (v2)**
  - Guixin Liu <kanie@linux.alibaba.com>
  - https://lore.kernel.org/linux-cxl/20260812060943.56246-1-kanie@linux.alibaba.com/
  - Needs review
    
- **cxl/region: Unregister the pmem region bridge on setup failure (v2)**
  - Guixin Liu <kanie@linux.alibaba.com>
  - https://lore.kernel.org/linux-cxl/20260812061043.57319-1-kanie@linux.alibaba.com/
  - Expecting v3.

- **cxl/pci: Skip reset detection for DVSEC emulated decoders (v3)**
  - Guixin Liu <kanie@linux.alibaba.com>
  - https://lore.kernel.org/linux-cxl/20260812082347.354371-1-kanie@linux.alibaba.com/
  - Needs review
    
- **PCI/CXL: Distinguish CXL capabilities in MCAP (v2)**
  - Penn <engguopeng@buaa.edu.cn> (v1 posted from peng.guo@montage-tech.com)
  - https://lore.kernel.org/linux-cxl/20260817075854.17207-1-engguopeng@buaa.edu.cn/
  - 2 patches: generic PCI MCAP definitions in `pci_regs.h` plus a
    `drivers/cxl/core/regs.c` filter so MMPT capabilities are not mistaken for CXL
    register blocks. Tested on CXL 1.1, CXL 3.0 and an MMPT-enabled device.
  - Needs review

- **cxl/mce: Avoid alias page retirement for corrected errors (v2)**
  - Shaikh Kamaluddin <shaikhkamal2012@gmail.com>
  - https://lore.kernel.org/linux-cxl/20260812151759.14390-1-shaikhkamal2012@gmail.com/
  - Single patch; extended-linear-cache MCE handling must not retire the alias page
    for corrected errors.
  - Pending v3.
    
- **cxl/test: Map mock device nodes to an online node (v1)**
  - Davidlohr Bueso <dave@stgolabs.net>
  - https://lore.kernel.org/linux-cxl/20260723180630.1302871-1-dave@stgolabs.net/
  - Needs review.

- **Fix hmat_adist_nb for CXL (v2)**
  - Alejandro Lucero Palau <alejandro.lucero-palau@amd.com>
  - https://lore.kernel.org/linux-cxl/20260716181114.35962-1-alejandro.lucero-palau@amd.com/
  - Single patch dropping `__meminitdata` from `hmat_adist_nb` so the notifier
    survives when CONFIG_MEMORY_HOTPLUG is off.
  - Rafael needs to pick up this.
 
### For next
- **cxl: Support Back-Invalidate (v7)**
  - Davidlohr Bueso <dave@stgolabs.net>
  - https://lore.kernel.org/linux-cxl/20260728144136.709882-1-dave@stgolabs.net/
  - v8 pending.

- **DCD Prep Series (v12)**
  - Anisa Su <anisa.su887@gmail.com>
  - https://lore.kernel.org/linux-cxl/20260731084901.1512819-1-anisa.su@samsung.com/
  - v13 pending. Will create a for-7.4 branch when ready.

- **DCD: Add support for Dynamic Capacity Devices (DCD) (v11)**
  - Anisa Su <anisa.su887@gmail.com>
  - https://lore.kernel.org/linux-cxl/20260625112638.550691-1-anisa.su@samsung.com/
  - Patches 1-8 split out.
  - Pending v12?

- **vfio/pci: Add CXL Type-2 device passthrough support (v4)**
  - Manish Honap <mhonap@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260813093631.2288172-1-mhonap@nvidia.com/
  - Needs review

- **PCI/CXL: Add CXL reset support for Type 2 devices (v10)**
  - Srirangan Madhavan <smadhavan@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260804192958.1823952-1-smadhavan@nvidia.com/
  - Pending v11. Needs review.

- **Support zero-sized HDM decoders (v9)**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260805055524.22311-1-icheng@nvidia.com/
  - Almost ready. Needs review tags.

- **cxl: Support mixed-granularity region interleaves (v3)**
  - Alison Schofield <alison.schofield@intel.com>
  - https://lore.kernel.org/linux-cxl/cover.1785444498.git.alison.schofield@intel.com/
  - Needs review.

- **Enable CXL PCIe Port Protocol Error handling and logging (v19)**
  - Terry Bowman <terry.bowman@amd.com>
  - Patches 1-5 applied for 7.3.
  - Rest needs Bjorn acks.
  - v20 pending.
    
- **cxl/region: Add cxl_decoder_is_passthrough() helper (v2)**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260805065935.27837-1-icheng@nvidia.com/
  - Discussion on going. Pushed back by Robert.

- **cxl/region: Reject delete of a provider-locked region (v1)**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260721055131.20935-1-icheng@nvidia.com/
  - Needs review.

- **tools/testing/cxl: Don't wrap cxl_core's own exported symbols (v1)**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260721084009.38100-1-icheng@nvidia.com/
  - Single patch.
  - Need review tags.
    
## v7.5 or later  
- **[RFC] cxl: Device protocol AER injection**
  - Terry Bowman <terry.bowman@amd.com>
  - https://lore.kernel.org/linux-cxl/20260717225700.3543801-1-terry.bowman@amd.com/
  - debugfs-based CXL protocol-error injection, written to provide a test procedure
    for the port-error series.
  - **Rework expected; no tags.** Dave Jiang asked for the definitions to move to
    `core.h`; Jonathan Cameron asked for a cover letter explaining the RFC status, a
    Documentation/ABI entry, and named constants instead of `sizeof(u32)`. Author
    prefers a new `core/ras_einj.c`. Gated on the port-error series landing.

- **cxl: Auto-create a region for Type-2 memdev attach (RFC v1)**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260805074042.30173-1-icheng@nvidia.com/
  - 3-patch RFC.
  - Discord discussion trending towards need to deal with interleaved regions and act same as type 3.
   
# July 21, 2026
## Agenda
* Opens
* cxl-cli
* QEMU
* v7.2 rc fixes
* v7.3 merge window
* v7.4 and beyond

## Opens

* Plumbers deadline for proposal July 24th (Davidlohr)

* Handling Sashiko pre-existing issue reports (Jonathan)
We'll keep revisiting this one.

* DAX extensions for private nodes (GregoryP)
How to get a private node.  Should we use a custom driver.
Should mem be hot plugged to normal numa node or private
numa node. I think Gregory will post code for folks to take
a look at option.

* FAMFS: the upstream push continues.
  DAX patches (in at 7.2) and fix-ups (queued for 7.3)


## CXL CLI

## NDCTL v85 was released
  https://github.com/pmem/ndctl/releases/tag/v85

## NDCTL v86
* Please test with the pending branch:
  https://github.com/pmem/ndctl/tree/pending

* v3 0/2 daxctl, util/sysfs: fix builtin-driver false failure on enable (ChenPei)
  - Ready to merge

* v7 0/5 ndctl: Dynamic Capacity additions for cxl-cli (AnisaS)
  - Companion to CXL driver set. Nice to review together.
  - Needs review.

* cxl/region: allow mixed-granularity regions (AlisonS)
  - Companion to CXL driver set. Nice to review together.
  - Needs review

* 0/3 ndctl: Add CXL region passthrough >16K gran test (AlisonS)
  - Companion to CXL driver set. Nice to review together.
  - Needs review

* test/fwctl: Add Get Feature OOB rejection regression test (RichardC)
  - Pending v2
* test/cxl-topology.sh: test zero-sized decoders (RichardC)
  - Needs review
* test/test/cxl-poison.sh: test scanning past fully mapped partitions (AlisonS)
  - Pending v2
* test/cxl-security.sh: test dimm unlock with a large serial number (AlisonS)
  - Maybe pending v2
* test/cxl-mbox: Regression test for huge CXL_MEM_SEND_COMMAND out.size (RichardC)
  - Needs review

* cxl/cli: HPA-ordered destroy-region teardown (PawelM)
  https://lore.kernel.org/linux-cxl/20260217082705.2475753-1-pawel.mielimonka@fujitsu.com/
  - Pending revision

* Enable CXL Protocol testing (TerryB)
  - RFC posted. Can we access that through a unit test run?


## QEMU
- v2 of new risc5 support and a bunch of other updates
- DaveJ asks status of 32 bit BAR issue w Fedora EDK, no update yet.

## v7.2 rc fixes
* None. Going forward will only include issues from merge window or critical bugs

## v7.3 merge window
### Fixes
- **Harden HDM decoder enumeration (v4)**
  - Alison Schofield <alison.schofield@intel.com>
  - https://lore.kernel.org/linux-cxl/cover.1784595445.git.alison.schofield@intel.com/
  - Review needed, on going

- **cxl: Sashiko bug fixes (v3)**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260708074228.43654-1-icheng@nvidia.com/
  - ready to apply?

- **cxl: Allow passthrough decoders with >16K granularity (v2)**
  - Alison Schofield <alison.schofield@intel.com>
  - https://lore.kernel.org/linux-cxl/cover.1783795720.git.alison.schofield@intel.com/
  - Review needed, on going.

- **cxl/hdm: Enforce CFMWS memory type policy at decoder commit time (v2)**
  - Mayank Rana <mayank.rana@oss.qualcomm.com>
  - https://lore.kernel.org/linux-cxl/20260711003341.2602368-1-mayank.rana@oss.qualcomm.com/
  - Pending v3.
  - Davidlohr - this may go away w BI support

- **cxl: Format the device serial number as unsigned (v2)**
  - Alison Schofield <alison.schofield@intel.com>
  - https://lore.kernel.org/linux-cxl/cover.1782948930.git.alison.schofield@intel.com/
  - Review needed, on going.

- **Fix hmat_adist_nb for CXL (v2)**
  - Alejandro Lucero Palau <alejandro.lucero-palau@amd.com>
  - https://lore.kernel.org/linux-cxl/20260716181114.35962-1-alejandro.lucero-palau@amd.com/
  - Rafael can pick up for ACPI sub-system.

- **cxl/region: Fix use-after-free in find_pos_and_ways() error path (v2)**
  - Alison Schofield <alison.schofield@intel.com>
  - https://lore.kernel.org/linux-cxl/20260711180755.1779002-1-alison.schofield@intel.com/
  - Posted v2 adding a second patch to use cleanup helper, please review

### Changes
- **DCD: Add support for Dynamic Capacity Devices (DCD) (v10)**
  - Anisa Su <anisa.su887@gmail.com>
  - https://lore.kernel.org/linux-cxl/cover.1779528761.git.anisa.su@samsung.com/
  - Looking for more reviews. Expecting v11.
    Consider picking up first 11 patches, basic hw setup, in 7.3 merge window.
    Expects to be adding tested by tags.

- **PCI/CXL: Add CXL reset support for Type 2 devices (v9)**
  - Srirangan Madhavan <smadhavan@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260709010304.680422-1-smadhavan@nvidia.com/
  - Review on going. Expecting v10.

- **Support zero-sized HDM decoders (v7)**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260708104024.68029-1-icheng@nvidia.com/
  - Review on going. Close to be ready to apply to next?

- **vfio/pci: Add CXL Type-2 device passthrough support (v3)**
  - Manish Honap <mhonap@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260625165407.1769572-1-mhonap@nvidia.com/
  - Review on going. Expecting v4.

- **cxl: Support mixed-granularity region interleaves (v2)**
  - Alison Schofield <alison.schofield@intel.com>
  - https://lore.kernel.org/linux-cxl/cover.1781199122.git.alison.schofield@intel.com/
  - Pending v3 with changes based on RobertR, RichardC, Sashiko.

- **cxl: Support Back-Invalidate (v5)**
  - Davidlohr Bueso <dave@stgolabs.net>
  - https://lore.kernel.org/linux-cxl/20260615145529.13848-1-dave@stgolabs.net/
  - Pending v6.

- **cxl/test: reject wrapped GET_LOG offsets (v1)**
  - Samuel Moelius <sam.moelius@trailofbits.com>
  - https://lore.kernel.org/linux-cxl/20260605142036.2062347-1-sam.moelius@trailofbits.com/
  - Expecting v2.
 
- **Enable CXL PCIe Port Protocol Error handling and logging (v18)**
  - Terry Bowman <terry.bowman@amd.com>
  - https://lore.kernel.org/linux-cxl/20260717222706.3540281-1-terry.bowman@amd.com/
  - Review on going. Need Bjorn acks for PCI commits.

- **cxl: Deny Features commands on the RAW mailbox path (v1)**
  - Dave Jiang <dave.jiang@intel.com>
  - https://lore.kernel.org/linux-cxl/20260715155126.1629178-1-dave.jiang@intel.com/
  - Ready to merge?

- **cxl: Convert PCIBIOS errors to errno on remaining DVSEC/PCIe accesses (v2)**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260623062002.21447-1-icheng@nvidia.com/
  - Could use more reviews.

- **cxl/test: Don't wrap cxl_core's own exported symbols**
  -  Richard Cheng <icheng@nvidia.com>
  -  https://lore.kernel.org/linux-cxl/20260721084009.38100-1-icheng@nvidia.com/
  -  Review please

- **cxl/region: Reject delete of a provider-locked region**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260721055131.20935-1-icheng@nvidia.com/
  - Need review. Pending v2 per Richard


## v7.4 and beyond (and others of interest)
- **CXL PM Init support**
  - Fabio M. De Francesco <fabio.m.de.francesco@linux.intel.com>
  - https://lore.kernel.org/linux-cxl/20260428182454.464655-1-fabio.m.de.francesco@linux.intel.com/T/#t
  - Pending v2

- **Private Memory NUMA Nodes (v5)**
  - Gregory Price <gourry@gourry.net>
  - https://lore.kernel.org/linux-cxl/20260720193431.3841992-1-gourry@gourry.net/

- **LSA 2.1 support for CXL**
  Neeraj
  https://lore.kernel.org/linux-cxl/1296674576.21772944201878.JavaMail.epsvc@epcpadp1new/
  - Pending v7. No activities for a long time. Still happening?

# June 15, 2026
## Agenda
* Opens
* cxl-cli
* QEMU
* v7.2 rc fixes
* v7.3 merge window
* v7.4 and beyond

## Opens
-  Poll for new times to be more Asia friendly.

## CXL CLI

* NDCTL v85 is next release, expect end of June
  Pending branch has been busy. Please test with it.
  https://github.com/pmem/ndctl/tree/pending

* NDCTL Test Runner
  Reminder: it is there doing daily and weekly runs
    Reports here: https://pmem.io/ndctl-test-runner/results.html
  Reminder: available for you to copy to set up like testing
    Repo here: https://github.com/pmem/ndctl-test-runner

* v2 cxl: Add CXL type2 accelerator unit test  (DaveJ)
  https://lore.kernel.org/nvdimm/20260515001203.2628149-1-dave.jiang@intel.com/
  - Companion to CXL driver set. Nice to review together. 
  - Needs review.

* v2 0/2 daxctl, util/sysfs: fix builtin-driver false failure on enable (ChenPei)
  https://lore.kernel.org/nvdimm/20260526132251.254476-1-cp0613@linux.alibaba.com/
  - Needs review.

* v6 0/7 ndctl: Dynamic Capacity additions for cxl-cli (AnisaS)
  https://lore.kernel.org/nvdimm/20260523095043.471098-1-anisa.su@samsung.com/
  - Companion to CXL driver set. Nice to review together.
  - Expecting a new rev.
  - Needs review.

* cxl/region: allow mixed-granularity regions (AlisonS)
  https://lore.kernel.org/nvdimm/fa5c109f08824180f58341ebd9055545a2ff3142.1780099216.git.alison.schofield@intel.com/
  - Companion to CXL driver set. Nice to review together.
  - Needs review

* 0/3 ndctl: Add CXL region passthrough >16K gran test (AlisonS)
  https://lore.kernel.org/nvdimm/cover.1781136221.git.alison.schofield@intel.com/
  - Companion to CXL driver set. Nice to review together.
  - Needs review

* cxl/cli: HPA-ordered destroy-region teardown (PawelM)
  https://lore.kernel.org/linux-cxl/20260217082705.2475753-1-pawel.mielimonka@fujitsu.com/
  - Pending revision

* Enable CXL Protocol testing (TerryB)
  - Pending rework for to use in cxl-test.


## QEMU
Performance fixup, adds a RAM region
risc v support in
Fedora , DaveJ issue, OVMF, still open
Initial UIO posted as RFC
Manish, vfio, RFC v2 is WIP

## v7.2 rc fixes
- **cxl: Convert remaining PCIBIOS errors to errno (v1)**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260607070241.48978-1-icheng@nvidia.com/
  - Need review

- **cxl/pci: Fix out-of-bounds read of the RAS Header Log (v1)**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260605041602.37944-1-icheng@nvidia.com/
  - Need review

- **cxl/mbox: Bound the output payload allocation to mailbox payload size (v1)**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260611094546.31496-1-icheng@nvidia.com/
  - Need review

- **cxl/features: Bound Get Feature read count to the output buffer (v1)**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260611094944.31638-1-icheng@nvidia.com/
  - Pending v2

- **cxl/ras: Fix match_memdev_by_parent() pointer type mismatch (v1)**
  - Terry Bowman <terry.bowman@amd.com>
  - https://lore.kernel.org/linux-cxl/20260608224319.587614-1-terry.bowman@amd.com/
  - Pending v2

- **cxl/port: Fix missing port lock in cxl_dport_remove() (v2)**
  - Terry Bowman <terry.bowman@amd.com>
  - https://lore.kernel.org/linux-cxl/20260608223533.583278-1-terry.bowman@amd.com/
  - Richard will bundle this fix with additional fixes?

- **cxl: Allow passthrough decoders with >16K granularity (v1)**
  - Alison Schofield <alison.schofield@intel.com>
  - https://lore.kernel.org/linux-cxl/cover.1781136281.git.alison.schofield@intel.com/
  - Need review

- **cxl: Fix MCE handler accessing issue for endpoint (v1)**
  - Dave Jiang <dave.jiang@intel.com>
  - https://lore.kernel.org/linux-cxl/20260616004007.4186004-1-dave.jiang@intel.com/T/#t
  - Need review

## v7.3 merge window
- **DCD: Add support for Dynamic Capacity Devices (DCD) (v10)**
  - Anisa Su <anisa.su887@gmail.com>
  - https://lore.kernel.org/linux-cxl/cover.1779528761.git.anisa.su@samsung.com/
  - Pending v11
  - Anisa working all the things :) Sashiko, DaveJ, coord w ndctl, etc...

- **Type2 device basic support (v27)**
  - Alejandro Lucero Palau <alejandro.lucero-palau@amd.com>
  - https://lore.kernel.org/linux-cxl/20260609215755.8685-1-alejandro.lucero-palau@amd.com/
  - Pending v28, Alejandro requested a check on implementation direction from Dan.
  https://lore.kernel.org/linux-cxl/50d8e423-8248-4e26-901b-010d14d22e67@amd.com/

- **Enable CXL PCIe Port Protocol Error handling and logging (v17)**
  - Terry Bowman <terry.bowman@amd.com>
  - https://lore.kernel.org/linux-cxl/20260505173029.2718246-1-terry.bowman@amd.com/
  - Pending v18. Need Bjorn acks.
  - Terry working Sashiko reported issues.

- **cxl: Add cxl_reset sysfs attribute for memdevs (v6)**
  - Srirangan Madhavan <smadhavan@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260528083154.137979-1-smadhavan@nvidia.com/
  - Review discussions on going. Pending v7.
  - Dan: PCI reset wants to run w/o drivers loaded. The cxl hdm code is all post
    driver load now. So pci core will need do/understand hdm mappings before CXL
    loads. (See the lore discussion)

- **cxl: Support Back-Invalidate (v5)**
  - Davidlohr Bueso <dave@stgolabs.net>
  - https://lore.kernel.org/linux-cxl/20260605003329.2584012-1-dave@stgolabs.net/
  - Needs review.

- **cxl: Support mixed-granularity region interleaves (v2)**
  - Alison Schofield <alison.schofield@intel.com>
  - https://lore.kernel.org/linux-cxl/cover.1781199122.git.alison.schofield@intel.com/
  - Needs review.
  - Terry to relay to RobertR.

- **cxl: Add CXL type2 accelerator support for cxl_test (v5)**
  - Dave Jiang <dave.jiang@intel.com>
  - https://lore.kernel.org/linux-cxl/20260611234409.256765-1-dave.jiang@intel.com/
  - Could use more review.

- **Support zero-sized HDM decoders (v4)**
  - Richard Cheng <icheng@nvidia.com>
  - https://lore.kernel.org/linux-cxl/20260607081345.61954-1-icheng@nvidia.com/
  - Need review. Pending v5. Alison asked for cxl_test test.
  - Dan: looking at spec, which decoders can be zero size. Will add to lore thread.

- **cxl/mbox: Support Media Operation (v2)**
  - Davidlohr Bueso <dave@stgolabs.net>
  - https://lore.kernel.org/linux-cxl/20260428200410.705675-1-dave@stgolabs.net/
  - Pending v3

- **cxl/test: reject wrapped GET_LOG offsets (v1)**
  - Samuel Moelius <sam.moelius@trailofbits.com>
  - https://lore.kernel.org/linux-cxl/20260605142036.2062347-1-sam.moelius@trailofbits.com/
  - Need review.

- **cxl: docs/linux/dax-driver - fix typos (v1)**
  - Zenghui Yu <zenghui.yu@linux.dev>
  - https://lore.kernel.org/linux-cxl/20260614161458.88942-1-zenghui.yu@linux.dev/
  - Ready to apply with v7.2-rc1 drop.

- **CXL PM Init support**
  - Fabio M. De Francesco" <fabio.m.de.francesco@linux.intel.com>
  - https://lore.kernel.org/linux-cxl/20260428182454.464655-1-fabio.m.de.francesco@linux.intel.com/T/#t
  - Pending v2

## v7.4 and beyond
* **LSA 2.1 support for CXL**
  Neeraj
  https://lore.kernel.org/linux-cxl/1296674576.21772944201878.JavaMail.epsvc@epcpadp1new/
  - Pending v7

- **RFC: PCI/CXL: Add RDPAS support for CXL.io (v1)**
  - Dave Jiang <dave.jiang@intel.com>
  - https://lore.kernel.org/linux-cxl/20260526182334.1010870-1-dave.jiang@intel.com/
  - Pending v2. Terry please review.
  - Enabling feature as spec defines. No platform intercept ATM.

# May 19, 2026
## Agenda
* Opens
* cxl-cli
* QEMU
* v7.1 rc fixes
* v7.2 merge window
* v7.3 and beyond

## Opens
* LFS/MM comments from those attended?
DCD meetup: made a plan for tags, deferred on hw interleaving

Anisa: another DCD use case is LMcache
https://github.com/LMCache/LMCache/blob/f9ce8cbe105bccfae81c82cdb5b32d57c7aba63a/docs/source/kv_cache/storage_backends/dax.rst?plain=1#L9

JohnG: FAMFS update
  2 implementations exist: fuse and stand-alone driver.
  bpf was shut down

Dan: Summary Session
  Handling port proto errors:
  Seems to be 2 panics, maybe only one is fatal (Dan), no both are fatal (Jon)

  Handling PCI details w CXL, like needing to know HDM config early and preserve
  it. ie reset. Learn CXL pieces early without having to load entire CXL driver.

(55 mins of chatter, see the transcript for more)

* Plumbers CFP (Davidlohr)
  Jul 24 first deadline for topics
  But registration date is July 10. Ask DavidL if you need early
  review on topic.

## CXL-CLI
* NDCTL v85 is next release, estimate end of June
  Pending branch has been busy. Please test with it.
  https://github.com/pmem/ndctl/tree/pending

* NDCTL Test Runner introduction:
  https://github.com/pmem/ndctl-test-runner
  https://pmem.io/ndctl-test-runner/results.html

* cxl: Add CXL type2 accelerator unit test
  https://lore.kernel.org/nvdimm/20260515001203.2628149-1-dave.jiang@intel.com/
  - Needs review

* daxctl: add support for famfs mode (JohnG)
  https://lore.kernel.org/nvdimm/0100019ddf06b207-eaf8cb8a-066e-4642-8947-effdb4848c20-000000@email.amazonses.com/
  - Pending revision

* test/daxctl-famfs.sh: test FAMFS mode transitions (JohnG)
  https://lore.kernel.org/nvdimm/0100019ddf06ce8f-c323d9cd-333b-4076-9717-7c80dbed7620-000000@email.amazonses.com/
  - Pending revision

* cxl/cli: HPA-ordered destroy-region teardown (PawelM)
  https://lore.kernel.org/linux-cxl/20260217082705.2475753-1-pawel.mielimonka@fujitsu.com/
  - Pending revision

* Enable CXL Protocol testing (TerryB)
  - test/cxl: Enable CXL protocol error testing using aer-inject
  - test/aer-inject: Add aer-inject correctable and uncorrectable internal error support
  - test/cxl: Force RAS status in cxl_handle_cor_ras() and cxl_handle_ras()
  - Pending rework for to use in cxl-test. 


## QEMU
Fedora issue w OVMF DaveJ reported. (link in discord chat)
Manish vfio cxl set
Joshua configurable cxl switches
  talk to cxl devices that are not part of current topo but are in fabric


## v7.1 rc fixes
* We are at v7.1-rc4

* cxl/hdm: fix a warning in devm_remove_action() (Sungwoo)
  https://lore.kernel.org/linux-cxl/20260420025547.1704692-2-iam@sung-woo.kim/
  - Needs review on v3

* cxl/test: Update mock memdev array before calling platform_device_add() (Ming)
  https://lore.kernel.org/linux-cxl/20260306-update_array_before_adding_mock_memdev-v1-1-a1a6af0952f1@zohomail.com/#t
  - Needs review

* cxl/region: Fix out of bounds access in cxl_cancel_auto_attach() (Ming)
  https://lore.kernel.org/linux-cxl/20260519-fix_out_of_bounds_access-v1-1-55fc60d83388@zohomail.com/
  - Needs review

* RiscV platform issue
  https://lore.kernel.org/linux-cxl/20260519015009.3430-1-cp0613@linux.alibaba.com/T/#t


## v7.2 merge window
*  Enable CXL PCIe Port Protocol Error Handling and Logging (Terry)
   https://lore.kernel.org/linux-cxl/20260505173029.2718246-1-terry.bowman@amd.com/T/#t
   - Pending v18
   - Needs PCI sub-system acks from Bjorn 

* Type2 device basic support (Alejandro)
  https://lore.kernel.org/linux-cxl/41e4bd2c-f11d-442f-bc3e-7198f2a2cd13@amd.com/T/#t
  - v26 review on going
  - Pending region resource release fix series from Dan

* Type2 cxl_test support (DaveJ)
  https://lore.kernel.org/linux-cxl/13c78882-d3df-474c-8232-6b22ef1b44ec@intel.com/T/#t
  - v2 pending v27 of Alejandro's patch series

* Save CXL HDM states across resets (Srirangan)
  https://lore.kernel.org/linux-cxl/69cee524adfd9_1b0cc6100f2@dwillia2-mobl4.notmuch/T/#t
  - pending v2

* Add cxl_reset sysfs attribute for PCI devices (Srirangan)
  https://lore.kernel.org/linux-cxl/20260306092322.148765-1-smadhavan@nvidia.com/T/#t
  - pending v6

* Add CXL Type-2 device passthrough support for VFIO (Manish)
  https://lore.kernel.org/linux-cxl/IA1PR12MB9030B0FABA316A0A3A3899E6BD202@IA1PR12MB9030.namprd12.prod.outlook.com/T/#t
  - pending v3

Add here or below:

Fabios series PM Init

DavidL back invalidate
	User control for back invalidate
	cxl-tests for back invalidate please


## v7.3 and beyond
* Add support for multiple DC regions RFC (Anisa)
  https://lore.kernel.org/linux-cxl/20260411013958.47422-1-anisa.su@samsung.com/
  - pending v1?

* LSA 2.1 support for CXL (Neeraj)
  https://lore.kernel.org/linux-cxl/1296674576.21772944201878.JavaMail.epsvc@epcpadp1new/
  - Pending v7

* Add support for mixed granularity regions (Alison)
  Merges Robert & Alison's previous work and extends coverage to both auto
  and user created region for every spec defined mixed gran config.
  Previous work:
     Allow 6 & 12 way regions on 3-way HB interleave (Alison)
     https://lore.kernel.org/linux-cxl/20250306232239.2609017-1-alison.schofield@intel.com/
     Support multi-level interleaving with smaller granularities for lower levels (Robert)
     https://lore.kernel.org/linux-cxl/20251028094754.72816-1-rrichter@amd.com/
  - Pending v1

# April 21, 2026
## Agenda
* Opens
* cxl-cli
* QEMU
* v7.1 rc fixes
* v7.2 merge window
* v7.3 and beyond

## Opens
* LPC Microconference - Jonathan call for topics.
  DavidL submitting microconference proposal.

## CXL-CLI
## NDCTL v85 is next release, estimate June
  - Pending branch has been busy. Please test with it where possible.

* Welcoming review:
  - Enable CXL Protocol testing (TerryB)
    with directional feedback from BenC.
  - test/cxl: Enable CXL protocol error testing using aer-inject
  - test/aer-inject: Add aer-inject correctable and uncorrectable internal error support
  - test/cxl: Force RAS status in cxl_handle_cor_ras() and cxl_handle_ras()

  Terry - won't need aer-inject if use cxl-cli inject commands, those
  cxl-cli will need to expand. See lore list mail thread.

* Waiting for revision:
  - daxctl: add support for famfs mode (JohnG)
  - test/daxctl-famfs.sh: test FAMFS mode transitions (JohnG)
  - cxl/cli: HPA-ordered destroy-region teardown (PawelM)
  - ndctl/lib: move nd_cmd_pkg with a flex array to end of structures (Alison)

## QEMU
- Release 11.0.0 is available.
- Qemu getting lots of security alerts, reduced to normal bugs because
  CXL emulation is not considered suitable for virtualization.
- RFC for Type2 support posted
- Fedora 43 new OVMF issue breaks CXL switch enumeration. DaveJ has
  work-around patch for QEMU.

## v7.1 rc fixes
* rc1 not released yet
* cxl/hdm: fix a warning in devm_remove_action() (Sungwoo)
  https://lore.kernel.org/linux-cxl/20260420025547.1704692-2-iam@sung-woo.kim/
  - Need review on v3
* cxl/test: Update mock memdev array before calling platform_device_add() (Ming)
  https://lore.kernel.org/linux-cxl/20260306-update_array_before_adding_mock_memdev-v1-1-a1a6af0952f1@zohomail.com/#t
  - Need review

## v7.2 merge window
*  Enable CXL PCIe Port Protocol Error Handling and Logging (Terry)
   https://lore.kernel.org/linux-cxl/20260302203648.2886956-1-terry.bowman@amd.com/
   - Pending v17
   - Need PCI sub-system acks from Bjorn 

* Type2 device basic support (Alejandro)
  https://lore.kernel.org/linux-cxl/20260330143827.1278677-1-alejandro.lucero-palau@amd.com/T/#t
  https://lore.kernel.org/linux-cxl/20260403210050.1058650-1-dan.j.williams@intel.com/T/#t
  - v25 review on going
  - DaveJ working on a set of patches that adds type2 emulation and regression
    testing for cxl_test.
  - v26 in the works integrating some of Dans proposals

* Support Media Operation (Davidlohr)
  https://lore.kernel.org/linux-cxl/20260518172303.0b210abe@jic23-huawei/T/#t
  - v2 review on going

* Support back-invalidate (Davidlohr)
  https://lore.kernel.org/linux-cxl/20260516022617.ioafhfccxvv5as3g@offworld/T/#t
  - v3 needs review

* PM Init completion for CXL reset (Fabio)
  https://lore.kernel.org/linux-cxl/20260428182454.464655-1-fabio.m.de.francesco@linux.intel.com/T/#t
  - pending v2

* Save CXL HDM states across resets (Srirangan)
  https://lore.kernel.org/linux-cxl/20260316172807.00000abc@huawei.com/
  - v2 pending?

* Add cxl_reset sysfs attribute for PCI devices (Srirangan)
  https://lore.kernel.org/linux-cxl/e6f9aaab-ce34-4a98-94e0-0d48fe78c5f4@intel.com/
  - v6 pending?

* Add CXL Type-2 device passthrough support for VFIO (Manish)
  https://lore.kernel.org/linux-cxl/IA1PR12MB9030B0FABA316A0A3A3899E6BD202@IA1PR12MB9030.namprd12.prod.outlook.com/T/#t
  - v2 under review

* LSA 2.1 support for CXL (Neeraj)
  https://lore.kernel.org/linux-cxl/1296674576.21772944201878.JavaMail.epsvc@epcpadp1new/
  - Pending v7

* Allow 6 & 12 way regions on 3-way HB interleave (Alison)
  https://lore.kernel.org/linux-cxl/20250306232239.2609017-1-alison.schofield@intel.com/
  - Pending v3

## v7.3 and beyond
* Support back invalidate (Davidlohr)
  https://lore.kernel.org/linux-cxl/20260315202741.3264295-1-dave@stgolabs.net/
  - v1 needs review

* RFC: Media Operations (Davidlohr)
  https://lore.kernel.org/linux-cxl/20260323204013.4010634-1-dave@stgolabs.net/
  - Non-RFC v2 pending

* RFC v2 workaround for CXL port PM init failure when ACS SV enabled (Fabio)
  https://lore.kernel.org/linux-cxl/20260311164145.498207-1-fabio.m.de.francesco@linux.intel.com/
  - Pending v3 and dropping RFC?

* Add support for multiple DC regions RFC (Anisa)
  https://lore.kernel.org/linux-cxl/20260411013958.47422-1-anisa.su@samsung.com/
  - in review, esp JohnG for FAMFS usage.
  

# March 17, 2026
## Agenda
* Opens
* cxl-cli
* QEMU
* v7.0 rc fixes
* v7.1 merge window
* v7.2 and beyond

## Opens
* CXL reset and decoder preservation
- Smitas fix protects the HPA range for DAX failover by not resetting
  decoders, but that protection is not actually needed, because we
  stopped unregistering regions on failure to assemble.

- May want to protect HPA ranges when userspace does hotplug on an
  auto region. ie so that same HPA range stays available.

- Type2 has defined CFMWS for Type2 that other regions cannot grab.
  Protecting the HPA range, the Type2 root decoders, is a problem that
  falls to Type2 driver alone?

* BIOS does not always program decoders for Type2. (Jonathan)
  Cannot know size of Type2 device.
  Jonathan suggests a code first ECN.
  This topic is now in the open.

* FAMFS is coming soon (JohnG)

* LSFMM what topics does this group want (Dan)

## CXL-CLI
## NDCTL v84 released March 14
  - https://github.com/pmem/ndctl/releases/tag/v84

## NDCTL v85 
* Welcoming review:
  - ndctl: Fix missing key_type parameter in ndctl_dimm_remove_key_stub (Chen)
  - ndctl: Fix meson configuration error when fwctl is disabled (Chen)
  - test/common: add helpers for CXL region replay testing (Alison)
  - test/cxl-region-replay.sh: add test of region replay workflow (Alison)

* Waiting for a revision:
  - cxl/cli: HPA-ordered destroy-region teardown (PawelM)
  - daxctl: Add support for famfs mode (JohnG)
  - test/daxctl-famfs.sh: test famfs mode transitions (JohnG)
  - ndctl/lib: move nd_cmd_pkg with a flex array to end of structures (Alison)

## QEMU
  - Michael picked up backlog, event updates, PPR, switch phys port series
    Expect to see in next QEMU release 11.0
  - Fast path KVM memory series waits til next release, Alireza.
  - Li Chen new series may be dup of Alireza's KVM.
  - Dropping I2C MCTP support and compliance capability, no interest.
    Dropping from Jonathans branch and no longer expected to go upstream.

## v7.0-rc Fixes
* Last PR v7.0-rc2
* Queued: expect rc6 PR
  - cxl/hdm: Avoid incorrect DVSEC fallback when HDM decoders are enabled
  - cxl/acpi: Fix CXL_ACPI and CXL_PMEM Kconfig tristate mismatch
  - cxl/region: Fix leakage in __construct_region()
  - cxl/port: Fix use after free of parent_port in cxl_detach_ep()

* Update mock memdev array before calling platfor_device_add() (Ming)
  https://lore.kernel.org/linux-cxl/20260306-update_array_before_adding_mock_memdev-v1-1-a1a6af0952f1@zohomail.com/
  - v1 needs reivew


## v7.1 merge window
* Queued:
  - ACPI: NUMA: Only parse CFMWS at boot when CXL_ACPI is on
  - cxl: Consolidate cxlmd->endpoint access (series)
  - cxl: cxl region changes for Type2 support (series)
  - cxl: Type 2 support preparation (series)


* Coordinate "Soft Reserved" handling with CXL and HMEM (Smita)
  https://lore.kernel.org/linux-cxl/20260210064501.157591-1-Smita.KoralahalliChannabasappa@amd.com/
  - v6 review ongoing. v7 coming?

*  Enable CXL PCIe Port Protocol Error Handling and Logging (Terry)
   https://lore.kernel.org/linux-cxl/20260302203648.2886956-1-terry.bowman@amd.com/
   - v16 review ongoing
   - Need PCI sub-system acks from Bjorn 

* Type2 device basic support (Alejandro)
  https://lore.kernel.org/linux-cxl/20260201155438.2664640-1-alejandro.lucero-palau@amd.com/
  - Two prep-sets applied to cxl/next
  - v23 review ongoing (original set)
  - PJs blocking issue seems fixed w Smita's reset patch.

* Save CXL HDM states across resets (Srirangan)
  https://lore.kernel.org/linux-cxl/20260316172807.00000abc@huawei.com/
  - v1 under review
  - See Dans latest comments here wrt where this belongs:
https://lore.kernel.org/linux-cxl/69b98960907e9_7ee31003b@dwillia2-mobl4.notmuch/

* Add cxl_reset sysfs attribute for PCI devices (Srirangan)
  https://lore.kernel.org/linux-cxl/e6f9aaab-ce34-4a98-94e0-0d48fe78c5f4@intel.com/
  - v5 under review

- Find Jonathan/Dan discussion starting at 11:25 in transcript posted on Discord.


* LSA 2.1 support for CXL (Neeraj)
  https://lore.kernel.org/linux-cxl/1296674576.21772944201878.JavaMail.epsvc@epcpadp1new/
  - Pending v7

* Add CXL Type-2 device passthrough support for VFIO (Manish)
  https://lore.kernel.org/linux-cxl/20260311203440.752648-1-mhonap@nvidia.com/
  - v1 under review

* cxl/test: Enable replay of user regions as auto regions (Alison)
  https://lore.kernel.org/linux-cxl/20260314061952.2221030-1-alison.schofield@intel.com/
  - v1 under review

* Use proper endpoint validity check upon sanitize. (Davidlohr)
  https://lore.kernel.org/linux-cxl/20260301221739.1726722-1-dave@stgolabs.net/
  - v1 under review

* Add endpoint decoder flags clear when PCI reset happens (DaveJ)
  https://lore.kernel.org/linux-cxl/abH9jumyX8ayixE4@aschofie-mobl2.lan/
  - pending v2. Can use some review.

* Do not return -EAGAIN from find_or_add_dport() for newly added dport (Ming)
  https://lore.kernel.org/linux-cxl/20260307-find_or_add_dport_not_return_eagain-v1-1-7c298461a9a4@zohomail.com/
  - v1 needs review

* Allow 6 & 12 way regions on 3-way HB interleave (Alison)
  https://lore.kernel.org/linux-cxl/20250306232239.2609017-1-alison.schofield@intel.com/
  - Pending v3

## v7.2 and beyond
* Support back invalidate (Davidlohr)
  https://lore.kernel.org/linux-cxl/20260315202741.3264295-1-dave@stgolabs.net/
  - v1 needs review

* RFC, workaround for CXL port PM init failure when ACS SV enabled (Fabio)
  https://lore.kernel.org/linux-cxl/20260311164145.498207-1-fabio.m.de.francesco@linux.intel.com/
  - Needs review and comments

* Zero sized decoder (VishalA)
  https://lore.kernel.org/linux-cxl/20251015024019.1189713-1-vaslot@nvidia.com/
  * No activity since 10/25?

* Add support for multiple DC regions RFC (Anisa)
  https://lore.kernel.org/linux-cxl/20260115102819.00006d55.alireza.sanaee@huawei.com/T/#t
  - pending DCD series as well. Anisa has taken over.
  - Anisa to send out RFC with discussion of approach.



# February 17, 2026
## Agenda
* Opens
* cxl-cli
* QEMU
* v7.0 rc fixes
* v7.1 merge window
* v7.2 and beyond


The full transcript is posted on the CXL Discord channel.

## Opens

- JonathanC: heads up on work coming wrt ras handling. 

- Anisa: DCD - FAMFS first user of DCD in simplest way, single region. 

- GregP: discussion about AI generated reviews. Welcomed if sender has reviewed
  the review and finds it valid.

- GregP: looking at external driver commonalities and refactoring. External drivers
  are type2, vfio, pmem, and reset, Do we pull exports out ahead and make common? 
  GregP will pull create region func first and post for comments.

- GregP: while testing is finding race conditions. Seems to be in refactored code
  not existing code. Try with MingL and DaveJ's latest race condition patchsets.

- Are there any vendors needing CXL physical hotplug to work properly upstream?
  Context: folks disabling ACS to make it work. Any customers or only a validation
  requirement? (Access Control Services - blocks CXL pm init for unexpected id)
  Users may be able to turn off this check specifically in ACS.

- JohnG: working set of DCD clarifications, expect to be an ECN
    and set of requirements around SW cache coherency - may be consortium confidential
    Both for consortium members (or others) to ask and review.

## CXL CLI
## NDCTL v84 - gathering for a Q1 release to support things through 7.0 kernel
   (non-CXL work mostly filtered out)

* Welcoming reviews:
  John's FAMFS Set:
  - v4 daxctl: Add support for famfs mode
  - v4 test/daxctl-famfs.sh: test famfs mode transitions
    https://lore.kernel.org/all/0100019bd34040d9-0b6e9e4c-ecd4-464d-ab9d-88a251215442-000000@email.amazonses.com/

  - v4 cxl/cli: enforce HPA-descending teardown (PawelM)
    https://lore.kernel.org/all/20260130121638.169160-1-pawel.mielimonka@fujitsu.com/

  - v2 test/cxl-poison.sh: test unaligned address translations in cxl_poison events (AlisonS)
    https://lore.kernel.org/all/20260115200241.522809-1-alison.schofield@intel.com/

  - v2 test/cxl-poison.sh: replace sysfs usage with cxl-cli cmds (AlisonS)
    https://lore.kernel.org/all/20260214023239.1352245-1-alison.schofield@intel.com/

* Waiting revisions:
  - FAMFS - awaiting update to unit test and fix for unit test fail (JohnG)

* Pending for v84: (many hiding on internal pending awaiting a coverity scan)
  Ben's Error Inject Set:
  - Documentation: Add docs for protocol and poison injection commands
  - cxl/list: Add injectable errors in output
  - cxl: Add poison injection/clear commands
  - cxl: Add inject-protocol-error command
  - libcxl: Add poison injection support
  - libcxl: Add CXL protocol errors
  - libcxl: Add debugfs path to CXL context
  DaveJ's ELC support
  - cxl: add support for extended linear cache
  New unit tests:
  - cxl/test: add test for extended linear cache support
  - cxl/test: add cxl-translate.sh unit test
  Updated unit tests:
  - test/cxl-topology.sh: test switch port target lists
  - test/cxl-poison.sh: add support for poison test for ELC
  - test/cxl-poison.sh: move cxl-poison.sh to use cxl_test auto region
  - test/cxl-poison.sh: fix cxl-poison.sh to detect the correct elc sysfs attrib


## QEMU
Tried to merge 20, got 2!!!
Rework of phys port control, allows reset testing.
JC wants to drop
    - compliance mailbox: no one uses and does nothing
    - i2c mctp not needed. Can use USB now.

## v7.0 rc fixes
* Fix nvdimm_bus race by cxl_nvdimm. (DaveJ)
  https://lore.kernel.org/linux-cxl/20260213224038.549798-1-dave.jiang@intel.com/
  - v3 under review. Some 0-day issues.

* Fix deadlock in cxl_memdev_autoremove() on attach failure. (Gregory)
  https://lore.kernel.org/linux-cxl/20260211192228.2148713-1-gourry@gourry.net/
  - Ready for cxl/fixes apply

* Avoid DVSEC fallback after region teardown. (Smita)
  https://lore.kernel.org/linux-cxl/20260212223800.23624-1-Smita.KoralahalliChannabasappa@amd.com/
  - v1 Need review

* Fix port enumeration failure. (Ming)
  https://lore.kernel.org/linux-cxl/20260212223800.23624-1-Smita.KoralahalliChannabasappa@amd.com/
  - v3 needs review

* Fix leakage in __construct_region(). (Davidlohr)
  https://lore.kernel.org/linux-cxl/20260202191330.245608-1-dave@stgolabs.net/
  - v1 needs review

* Fix testing of decoder flags as bitmasks. (Alison)
  https://lore.kernel.org/linux-cxl/20260206181404.1025991-1-alison.schofield@intel.com/
  - Pending v2 to split the patch, could use more review tags.

## v7.1 merge window
* Coordinate Soft Reserved handling with CXL and HMEM. (Smita)
  https://lore.kernel.org/linux-cxl/20260210064501.157591-1-Smita.KoralahalliChannabasappa@amd.com/
  - v6 needs review

* CXL port error protocol handling and logging. (Terry)
  https://lore.kernel.org/linux-cxl/20260203025244.3093805-1-terry.bowman@amd.com/
  - Pending v16
  - Patch series split into 3 parts with 1 and 2 in 7.0 now.
  - Need tags from Bjorn for PCI bits

* Pull region specific logic into new files. (Gregory)
  https://lore.kernel.org/linux-cxl/20260211204206.2171525-1-gourry@gourry.net/
  - v3 needs review

* Type 2 device basic support. (Alejandro)
  https://lore.kernel.org/linux-cxl/20260201155438.2664640-1-alejandro.lucero-palau@amd.com/
  - v23 needs review
  - Also waiting on testing feedback from PJ
  - No functional dependency. In Dans queue to review.

* Explicit DAX driver selection and hotplug. (Gregory)
  https://lore.kernel.org/linux-cxl/20260129210442.3951412-1-gourry@gourry.net/
  - wait on v2 reworks to review

* Delay insert iomem resource. (Alison)
  https://lore.kernel.org/linux-cxl/20260212062250.1219043-1-alison.schofield@intel.com/
  - Need review
  
* Allow 6 & 12 way regions on 3-way HB interleave (Alison)
  https://lore.kernel.org/linux-cxl/20250306232239.2609017-1-alison.schofield@intel.com/
  - Pending v3

* LSA 2.1 support for CXL pmem (Neeraj)
  https://lore.kernel.org/linux-cxl/20260123113112.3488381-1-s.neeraj@samsung.com/#r
  - Need response to question from Dan: https://lore.kernel.org/linux-cxl/6979522dc1916_1d331009@dwillia2-mobl4.notmuch/
  - Need to address cxl_test cxl-topology.sh regression test issue
  - Question for call: is the no interleave limitation OK?
    Neeraj: what is the impact of adding interleave with first merge? 

## v7.2 and beyond
* Zero sized decoder (VishalA)
  https://lore.kernel.org/linux-cxl/20251015024019.1189713-1-vaslot@nvidia.com/
  * No activity since 10/25?

* VFIO CXL type2 support RFCv2 (Manish)
  https://lore.kernel.org/linux-cxl/20251209165019.2643142-1-mhonap@nvidia.com/T/#t
  - Manish has a new set coming for review.

* CXL type2 reset support v4 (Srirangan)
  https://lore.kernel.org/linux-cxl/20260120222610.2227109-1-smadhavan@nvidia.com/
  - Pending v5? Not much response from submitter on the series.

* Add support for multiple DC regions RFC (Anisa)
  https://lore.kernel.org/linux-cxl/20260115102819.00006d55.alireza.sanaee@huawei.com/T/#t
  - pending DCD series as well. Anisa has taken over.
  - Anisa to send out RFC with discussion of approach.

# January 20, 2026

## Agenda
* Opens
* cxl-cli
* QEMU
* v6.19 rc fixes
* v7.0 merge window
* v7.1 and beyond

## Opens
- Dan suggested, Jonathan seconded, we need to use this forum for tech topics.
  We typically have the last 30 mins to dive into such topics.

- Today, ManishH shared his work and plans for the vfio-cxl driver.

## CXL CLI
## NDCTL v84 - gathering for a Q1 release to support things through 7.0 kernel

* Welcoming reviews:
  - v6 Add error injection support (BenC)
    https://lore.kernel.org/nvdimm/20260109160720.1823-1-Benjamin.Cheatham@amd.com/
     
  - v2 cxl/test: test unaligned address translations in cxl_poison events (AlisonS)
    https://lore.kernel.org/nvdimm/20260115200241.522809-1-alison.schofield@intel.com/

  - v2 cxl/cli: HPA-ordered destroy-region teardown (PawelM)
    https://lore.kernel.org/linux-cxl/20260120143212.3006273-1-pawel.mielimonka@fujitsu.com/

  - v2 daxctl: replace basename() usage with new path_basename() (AlisonS)
    https://lore.kernel.org/nvdimm/20260116043056.542346-1-alison.schofield@intel.com/

  - util/sysfs: add hint for missing root privileges on sysfs access (AlisonS)
    https://lore.kernel.org/nvdimm/b74bfd8623fcfc4cf1078991b22b8c899147f5fb.1768530600.git.alison.schofield@intel.com/
    
* Waiting revisions:

* Pending for v84:
  - test/cxl-topology.sh: test switch port target lists
  - cxl/test: add support for poison test for ELC
  - cxl/test: add test for extended linear cache support
  - cxl/test: move cxl-poison.sh to use cxl_test auto region
  - cxl/test: fix cxl-poison.sh to detect the correct elc sysfs attrib
  - cxl: add cxl-translate.sh unit test
  - ndctl/test: fully reset nfit_test in pmem-ns unit test
  - add support for extended linear cache
  - README.md: exclude unsupported distros from Repology badge

## QEMU
Soft freeze in March. 
- event, memory repair, fixes awaiting merge
- back-invalidate is new, tested, ready to merge (DavidL) 
- next up: phys port control for switches 
- Adress lookup changes
- GregP looking for KVM support fix, it's in work by Ali

## v6.19 rc fixes
* Fixes merged in for rc6

## v7.0 merge window
* Pending in cxl/next:
345f23df5d08 cxl/region: Use do_div() for 64-bit modulo operation
78b50b598462 cxl/region: Translate HPA to DPA and memdev in unaligned regions
19885bd10755 cxl/region: Translate DPA->HPA in unaligned MOD3 regions
5c604e7a9f6c cxl/core: Fix cxl_dport debugfs EINJ entries
b4692385bb68 cxl/acpi: Remove cxl_acpi_set_cache_size()
0db2344eb8a8 cxl/hdm: Fix newline character in dev_err() messages
7b4f9743fbbf cxl/pci: Remove outdated FIXME comment and BUILD_BUG_ON
fa19611f96fd Documentation/driver-api/cxl: device hotplug section
63e5a6294dad Documentation/driver-api/cxl: BIOS/EFI expectation update

  - Make ELOG and GHES log and trace consistently (Fabio)
    https://lore.kernel.org/linux-cxl/CAJZ5v0g80j4iFMXYDKek8VBYsa0g35avvw+UK6RxutcmxSX+WA@mail.gmail.com/T/#t
    - Picked up by Rafael

* Linus declared rc8. All series want to merge should be settled by next week.

* Enable CXL PCIe port protocol error handling and logging (Terry)
  https://lore.kernel.org/linux-cxl/20260114182055.46029-1-terry.bowman@amd.com/T/#t
  - Pending v15 with some discussions still on going in v14
  - Intend to split up for manageable review and merge.

* Support soft reserve (Smita)
  https://lore.kernel.org/linux-cxl/20250822034202.26896-1-Smita.KoralahalliChannabasappa@amd.com/T/#t
  - Pending next rev?
  
* Zen5 PRM translation support (Robert)
  https://lore.kernel.org/linux-cxl/20250822034202.26896-1-Smita.KoralahalliChannabasappa@amd.com/T/#t
  https://lore.kernel.org/linux-cxl/20260112111707.794526-1-rrichter@amd.com/T/#t
  - Waiting on requested addition for the Convention doc from Dan
  - Upstream PRM usage objection from PeterZ settled?

* Introduce cxl_region_driver field for cxl_region (Gregory)
  https://lore.kernel.org/linux-cxl/aWfe-r7uEV-ajfhX@gourry-fedora-PF4VCD3F/T/#t
  - Needs review
  - Maybe hold off on patch 1, the ABI intro. Take 2,3 that move code only.

## v7.1 and beyond
* Type 2 accelerator basic support v22 (Alejandro)
  https://lore.kernel.org/linux-cxl/20251205115248.772945-1-alejandro.lucero-palau@amd.com/T/#t
  - Pending debug with PJ before v23
  - Alejandro asking for eyes on region config from BIOS

* Support Low Memory Hole v6 (Fabio)
  - Pending resolve of cxl_test support
  - Will move region driver to module, outside of cxl core

* LSA 2.1 label support for CXL v5 (Neeraj)
  https://lore.kernel.org/linux-cxl/20260109124437.4025893-1-s.neeraj@samsung.com/T/#t
  - Need acks from Ira for nvdimm changes
  - Review ongoing

* Allow 6 & 12 way regions on 3-way HB interleave (Alison)
  https://lore.kernel.org/linux-cxl/20250306232239.2609017-1-alison.schofield@intel.com/
  - Pending v3

* VFIO CXL type2 support RFCv2 (Manish)
  https://lore.kernel.org/linux-cxl/20251209165019.2643142-1-mhonap@nvidia.com/T/#t
  - Needs review

* CXL type2 reset support v3 (Srirangan)
  https://lore.kernel.org/linux-cxl/aW1ercZMyqh2Ej5F@aschofie-mobl2.lan/T/#t
  - Needs review

* Add support for multiple DC regions RFC (Anisa)
  https://lore.kernel.org/linux-cxl/20260115102819.00006d55.alireza.sanaee@huawei.com/T/#t
  - Of course pending DCD series as well
  - Anisa - Happy to take over, rebase, add Fans fixes. Needs more work done on solidifying 
    how to expose this.
  - Dan - risk is merging ABI that we are not commited to. 
  - Anisa to send out RFC with discussion of approach.

* CXL .cache device support RFCv2 (Ben)
  https://lore.kernel.org/linux-cxl/20251111214032.8188-1-Benjamin.Cheatham@amd.com/T/#t 
  - Needs confirmed user for support
  - Going through reviews

## Patch series of interest to CXL
* FAMFS v7 (John)
  https://lore.kernel.org/linux-cxl/0100019bdbf77d8b-fc329dba-dc0d-4233-9b6a-b45e3e271727-000000@email.amazonses.com/T/#t

* Move dax_pgoff_to_phys() v4 (John)
  https://lore.kernel.org/linux-cxl/20260114213209.29453-2-john@groves.net/T/#t

* N_PRIVATE support RFCv3 (Gregory)
  https://lore.kernel.org/linux-cxl/20260108203755.1163107-1-gourry@gourry.net/T/#t

* Add runtime hotplug state control v2 (Gregory)
  https://lore.kernel.org/linux-cxl/df04f6e2-39ee-46f0-984d-54dcba16a011@kernel.org/T/#t

* Identify the accurate NUMA ID of CFMWS v2 (Cui)
  https://lore.kernel.org/linux-cxl/20260106031042.1606729-1-cuichao1753@phytium.com.cn/T/#t


# October 28, 2025

## Agenda
* Opens
* cxl-cli
* QEMU
* v6.18 rc fixes
* v6.19 merge window
* v6.19 and beyond

## Opens
* FAMFS JohnG
  - Adding sw managed cache coherency, leveraging libpmem?
  - Jonathan - caution about archs that do not describe CXL
    flush behavior because CXL did not exist.
* GregP lead discussion of new Anon ZONE_DEVICE allocator
  - whether it should use existing DAX + memory_hotplug
  - whether it should use something like hugetlb allocator
  - whether pgmap->alloc_folio makes sense
  - whether existing buddy-allocator could be extended for "arenas"
  - Maybe will discuss more at plumbers

## CXL CLI
## NDCTL v83 released September 30th
  - https://github.com/pmem/ndctl/releases/tag/v83
## NDCTL v84 and beyond
* Welcoming reviews:
  - ndctl: v2 Add error injection support (BenC)
  - cxl/test: add cxl-translate unit test (AlisonS)
  - Introduce sanitize-memdev functionality (DavidLohrB)
  - Add support for extended linear cache (DaveJ)
* Waiting revisions:
  - test: fail on unexpected kernel error & warning, not just "Call Trace" (MarcH)
  - test/common: document magic number CXL TEST QOS CLASS=42 (MarcH)
  - test/monitor.sh: replace sleep with event driven wait (AlisonS)
* Merged to pending for v84:
  - README.md: exclude unsupported distros from Repology badge (AlisonS)

## QEMU
- last pull request missed some cxl changes, expect in a second set,
  will include event, sanitize,
- hw/cxl: Add a performant (and correct) path for the non interleaved cases.
- DavidL sighting crash using cxl mem and vfio, running w kvm. Jonathan says
  not supported - don't do that w kvm. No guardrails around that yet.


## v6.18 rc fixes
* A number of fixes for 6.18-rc2 merged. Mostly extended linear cache related.
* Generic Initiator device handle fix (Shuai)
  - Queued

## v6.19 merge window
* Will start cxl/next on 6.18-rc4 next week. Below are ready to be queued
  - Remove page-allocator quirk section for CXL doc (Gregory)
  - Remove devm_cxl_port_enumerate_dports (Ming)
  - Fix typo in cdat.c (Alok)
  - Add a loadable module for address translation series (Alison)

* Add managed SOFT RESERVE resource handling (Smita)
  https://lore.kernel.org/linux-cxl/aQAmhrS3Im21m_jw@aschofie-mobl2.lan/T/#t
  - v3 discussion on going
  - Pending v4

* Enable CXL PCIe port protocol error handling and logging (Terry)
  https://lore.kernel.org/linux-cxl/20250925223440.3539069-1-terry.bowman@amd.com/T/#t
  - Pending v13

* Type2 device support (Alejandro)
  https://lore.kernel.org/linux-cxl/9a3eed68-9394-4f87-a204-4f2a0caf496e@intel.com/T/#t
  - v19 review on going
  - Now has dependency on Terry's protocol error set
  - Pending v20
  - Can use a check from Dan

* Low Mem Hole (Fabio)
  https://lore.kernel.org/linux-cxl/20251006155836.791418-1-fabio.m.de.francesco@linux.intel.com/T/#t
  - Convention doc in cxl/next
  - v5 review on going
  - cxl_test support discussion on going

* ACPI PRM Address Translation support - Zen5 (Robert)
  https://lore.kernel.org/linux-cxl/aNITd1fXcBxKM5mF@gourry-fedora-PF4VCD3F/T/#t
  - needs convention doc
  - pending v4

* CXL LSA 2.1 labeling support (Neeraj)
  https://lore.kernel.org/linux-cxl/aNMnmdOY4g5PRpxY@aschofie-mobl2.lan/T/#t
  - pending v4
  - Can use some review

* Support zero sized decoder (Vishal A)
  https://lore.kernel.org/linux-cxl/20251015024019.1189713-1-vaslot@nvidia.com/T/#t
  - pending v2

* Add handling of locked CXL decoders (Dave)
  https://lore.kernel.org/linux-cxl/637292ff-0cca-41bd-8ce9-4e38d6b1ff1b@intel.com/T/#t
  - review on going

* hmat_register_target() lockdep issue (Dave)
  https://lore.kernel.org/linux-cxl/20251017212105.4069510-1-dave.jiang@intel.com/T/#t
  - v3 review on going

* Add support to indicate extended linear cache is present via sysfs attribute (Dave)
  https://lore.kernel.org/linux-cxl/20251028144125.0000133b@huawei.com/T/#t
  - v3 review on going

* Adjust extended linear cache failure emission (Dave)
  https://lore.kernel.org/linux-cxl/20251003185509.3215900-1-dave.jiang@intel.com/
  - v2 needs review

* Support multi-level interleaving with smaller granularities for lower levels (Robert)
  https://lore.kernel.org/linux-cxl/20251028094754.72816-1-rrichter@amd.com/
  - needs review
  - platform exists, BIOS setup regions only ATM
  - look at Alison's Allow 6 & 12 way regions on 3-way HB interleave patch

* Make ELOG and GHES log and trace consistently (Fabio)
  https://lore.kernel.org/linux-cxl/SJ1PR11MB60836FB0D4D8EE564759F7E3FCFCA@SJ1PR11MB6083.namprd11.prod.outlook.com/T/#t
  - v6 review on going

* Translate DPA->HPA in unaligned MOD3 regions (Alison)
  https://lore.kernel.org/linux-cxl/20251014062850.727428-1-alison.schofield@intel.com/
  - v2 needs review
  - Jonathan maybe waiting for v3 w bot fix :(

* Allow 6 & 12 way regions on 3-way HB interleave (Alison)
  https://lore.kernel.org/linux-cxl/20250306232239.2609017-1-alison.schofield@intel.com/
  - Pending v3
  - look at Robert' multi-level interleave patch

* Coherent Cache Management System (Jonathan)
  https://lore.kernel.org/linux-cxl/20251023133136.00006cdd@huawei.com/T/#t
  - pending v5
  - Will go through ARM SOC tree
  - No one wants it ?

* CXL.mem error isolation support (Ben)
  https://lore.kernel.org/linux-cxl/20250730214718.10679-1-Benjamin.Cheatham@amd.com/T/#t
  - Review on going
  - No expected user yet

* CXL reset support for devices. (Srirangan / Vishal A)
  https://lore.kernel.org/linux-cxl/20250221043906.1593189-1-smadhavan@nvidia.com/
  - Pending v3
  - VishalA has taken over

## v6.20 and beyond
* Initial CXL.cache device support (Ben)
  - Testing and planning a RFC-v2.
* Initial support for Back-Invalidate (DavidLohrB)
  - Has dependencies on Type2. Consider cherry-picking a couple of T2 patches
    related to region creation if T2 not landing soon.
* Hotness Driver (Jonathan)
* DCD
  - Continue to wait for an upstream user to take over
  - JohnG: most likely famfs is that first upstream user
* fwctl support for CCI switch (Jonathan)


# September 30 2025

* Opens
* cxl-cli
* QEMU
* v6.18 merge window
* v6.18 rc fixes
* v6.19 merge window
* v6.19 and beyond

## Opens
* John Groves - submitting plumbers topics:
- DCD: the namespace for composable memory.
- FAMFS Update - DAX challenges and use cases
- John Groves to post a detailed decription of DAX issues w FAMFS.
- VishalA working a patch to not fail on 0 size committed and locked decoders
- DaveJ asks that you base patches on the RC that cxl/next is based upon, not upon cxl/next directly. Include the base commit in the patch. Comment non-compliance ;)

## CXL CLI
## NDCTL v83
* Release is WIP, perhaps today
  - Last commit removes libtracefs build dependency that broke v80,81,82.
  - https://github.com/pmem/ndctl/commits/pending/

## NDCTL v84 and beyond
* Reviews welcome:
  - cxl/test: add cxl-translate unit test (expect need for 6.18 kernel) (AlisonS)
  - Introduce sanitize-memdev functionality (DavidLohrB)
* Revisions welcome:
  - ndctl: v2 Add error injection support (BenC)
  - test: fail on unexpected kernel error & warning, not just "Call Trace" (MarcH)
  - ndctl,cxl/test: Add a common unit test for creating pmem namespaces(AlisonS)
  - test/monitor.sh: replace sleep with event driven wait (AlisonS)

## QEMU
* Features pending next release, reviews welcome.

## v6.18 merge window
* Window open this week. Will send PR end of week or early next week.

## v6.18 rc fixes
* Avoid missing port component registers setup (Ming)
  - Can use review.

## v6.19 merge window
* Add managed SOFT RESERVE resource handling (Smita)
  - v3 now available for review
* Enable CXL PCIe port protocol error handling and logging (Terry)
  - v12 going through reviews
* Type2 device support (Alejandro)
  - pending v19?
  - please review
* Low Mem Hole (Fabio)
  - Convention doc in cxl/next
  - v5 to be posted soon
* ACPI PRM Address Translation support - Zen5 (Robert)
  - needs review
  - needs convention doc
  - pending v4
* CXL LSA 2.1 labeling support (Neeraj)
  - v3 needs review
* CXL.mem error isolation support (Ben)
  - Is there a pending use case?
* CXL reset support for devices. (Srirangan)
  - Pending v3
  - VishalA has taken over
* Remove devm_cxl_port_enumerate_dports() (Ming)
  - Queued to cxl/next after merge window
* Make ELOG and GHES log and trace consistently (Fabio)
  - v5 to be posted soon
* Allow 6 & 12 way regions on 3-way HB interleave (Alison)
  - Pending v3
* Translate DPA->HPA in unaligned MOD3 regions (Alison)
  - v1 posted, need review
* CXL: Add a loadable module for address translation (Alison)
  - v2 needs review

## v6.20 and beyond
* Initial CXL.cache device support (Ben)
- Testing and planning a RFC-v2.
* Hotness Driver (Jonathan)
- DavidLohrB - are you modifying your apporach to use K promote B stuff.
- David has a proposal to co-exist.
- Qemu support?  Jonathan has something functional, welcomes more.
* non-x86 cache flushing ("wbinv") (Jonathan)
- v4 WIP, has substantial feedback from DanW. Got ACK from ARM folks.
* DCD
- User? IraW posted a rebase for Micron person who asked on Discord.
- John Groves submitting plumbers topic for DCD: the namespace for composable memory.
* cxl: Initial support for Back-Invalidate (DavidLohrB)
- Available to review. Expect session at plumbers too.
* fwctl support for CCI switch (Jonathan)


# September 2025
* Opens
* cxl-cli
* QEMU
* v6.17 rc fixes
* v6.18 merge window
* v6.19 and beyond

## Opens
- DanW - Early topic acceptance mid month.
- JonathanC - Plumbers device memory microconf topic submit deadline Sept30.
- DavidLohrB - back invalidate and cxl.cache topic plumbers proposal coming soon
- famfs (JohnGroves) - needs to clean up issue w Alistairs dax set.
  Dax never clears the page mapping or folio mapping. John expects
  to send fixup. Also topic for next DCD call.


## CXL CLI
## NDCTL v83
* See queue in https://github.com/pmem/ndctl/tree/pending
  - expect September release

## NDCTL v83 (maybe) and beyond:
* Introduce sanitize-memdev functionality (DavidLohrB)
  - Needs review
  - Received a couple of it "works for me" replies but no review tags.
* ndctl: v2 Add error injection support (BenC)
  - status ?
* test: fail on unexpected kernel error & warning, not just "Call Trace" (MarcH)
  - Needs next rev on check dmesg piece
  - Needs next rev on kmesg piece
* test/cxl-poison.sh: test inject and clear poison by HPA (AlisonS)
  - Needs review
* cxl/test: add cxl_translate unit test (AlisonS)
  - Needs review
* ndctl,cxl/test: Add a common unit test for creating pmem namespaces(AlisonS)
  - Pending a v2
* test/monitor.sh: replace sleep with event driven wait (AlisonS)
  - Pending a v4


## QEMU
- Fan support mv'd to hobby level
- Looking for more reviewers - wonderful role, not for poor souls
- 10.1 released
- 10.2
    - event record and RAS features
    - ARM SPSA support needs reviewers
    - a bunch more in the works



## v6.17 rc fixes
* None

## v6.18 merge window
* Add managed SOFT RESERVE resource handling (Smita)
  - New patch series, v1 under review
* Enable CXL PCIe port protocol error handling and logging (Terry)
  - v11 going through reviews
* Delayed dport creation (Dave)
  - v9 needs review
* Update CXL access coordinates to node directly (Dave)
  - v3 needs acks from Rafael
  - Update maintainers
* Type2 device support (Alejandro)
  - Main issues brought up by Alejandro at this link:
  - Waiting on response: https://lore.kernel.org/linux-cxl/e74a66db-6067-4f8d-9fb1-fe4f80357899@amd.com/T/#me74adadf01d65ea15b5ef92a3947f8730f06ec93
  - Wants to address those things before posting next version
* Low Mem Hole (Fabio)
  - Posted CXL convention doc, going through review
  - v4 under review
* Zen5 translate part 2 (Robert)
  - need review
  - need convention doc
  - Robert gave overview on layers of patches, refactors to Zen5 support.
* CXL.mem error isolation support (Ben)
  - need review
  - Is there a pending use case?
* CXL LSA 2.1 labeling support (Neeraj)
  - v2 needs review and response to review comments
* CXL reset support for devices. (Srirangan)
  - Pending v3
  - still active?
* Allow 6 & 12 way regions on 3-way HB interleave (Alison)
  - Pending v3
* (RFC) Translate DPA->HPA in unaligned MOD3 regions (Alison)
  - Pending v1
* Make ELOG and GHES log and trace consistently (Fabio)
  - Pending v5
* CXL: Add a loadable module for address translation (Alison)
  - v2 needs review

* anything else missed?

## v6.19 and beyond
* Initial CXL.cache device support (Ben)
* Hotness Driver (Jonathan)
  Driver level is not critical path, what we do with hotness data in the
  kernel level is.
* non-x86 cache flushing ("wbinv") (Jonathan)
  - Cache coherency management subsystem
  - non x86 folks, please try it out.
* DCD
  - next call will not be cancelled
* vfio-cxl type 2 (Zhi)
  - Still pending v2 RFC. Abandoned?
* cxl: Initial support for Back-Invalidate (DavidLohrB)
* fwctl
  - Jonathan shared update on fwctl future. A new kconfig is needed for
    CCI switch support. These go beyond the security scope of current FWCTL.
    Plan is to convert the commands to Features in order to utilize FWCTL.
    Some ops will be encapsulated as a Feature commands.


# August 2025
* Skipped

# July 2025
* Opens
* cxl-cli
* QEMU
* v6.16 rc fixes
* v6.17 merge window
* v6.18 and beyond

## Opens
- JohnG: wrt dax device w dax extents, hope to get a smaller
  group call. Ira going to do a poll to select time and set up.
- JohnG: famfs v2 patches, need to fixup dax, heads up need dax developer help
- DavidLohr: Background handling discussions ongoing. More feedback welcome.


## CXL CLI
* NDCTL v82 was released June 12
  https://github.com/pmem/ndctl/releases/tag/v82
* Patch Queue for v83:
* ndctl: Add missing test dependencies and other fixups (DanW)
  - Set applied to pending
* Introduce sanitize-memdev functionality (DavidLohrB)
  - Received a couple of it "works for me" replies but no review tags.
* ndctl: v2 Add error injection support (BenC)
  - Needs review
* cxl: Add helper function to verify port is in memdev hierarchy (DaveJ)
  - next rev pending
* test: fail on unexpected kernel error & warning, not just "Call Trace" (MarcH)
  - Needs review
* test/cxl-poison.sh: test inject and clear poison by HPA (AlisonS)
  - Pending a v2 with added cases but reviews still welcome on v1.
* Documentation: cxl,daxctl,ndctl add --list-cmds info (RongT)
  - Pending Alison to apply - is good.
* test/monitor.sh: replace sleep with event driven wait (AlisonS)
  - Pending a v4
* ndctl: Various typos fix in Documention/, cxl/, ndctl/, ... (YiZ)
  - Pending a v3
* ndctl: Dynamic Capacity additions for cxl-cli (IraW)
  - Deferred but not forgotten

## QEMU

Jonathan not in mtg. DaveJ covering...

* QEMU 10.1 soft freeze is on the 15th July (1 week from today).
Queued up waiting for Michael Tsirkin to get to:
  - FM-API DCD support (Anisa)
Waiting for ARM maintainers
  - ARM-virt - one open question around the address space allocator used for RCRBs.

In good state so maybe if we get enough review we can try to slip in late
this week:  (please review!)
- 3.2 Event injection updates (Shiju)
- Maintenance commands (Davidlohr and Shiju)

Longer term stuff
- Interest in an upstream MHD implementation, so revisit inter 'host'
  communication path (Gregory)
- MCTP over USB - worked for Anisa so need to resolve remaining issues
  (MTU not being respected from device to host) and separate from
  stalled MCTP over I2C
- CHMU.  Works etc, but little point in upstreaming yet.
- ARM SBSA reference platform support (separate RC) - Waiting for SBSA
  and PCI maintainers to review.
- Performance path for non interleaved case. Useful, needs cleaning up
  and tear down support -> Similar support needed for virtualized DCD.
- Various other sets awaiting new versions.


## v6.16 rc fixes
* rc4 PR with some fixes accepted
* CXL Feature: Using full data transfer only when offset is 0 (Ming)
  - Waiting on Jonathan to inquire spec clarification with the consortium
* Fix wrong dpa checking in PPR operation (Ming)

## v6.17 merge window in cxl/next
* Documentation/driver-api/cxl: Introduce conventions.rst
* Documentation: cxl: fix typos and improve clarity in memory-devices.rst
* cxl/pci: Replace mutex_lock_io() w mutex_lock() for mailbox access
* cxl_test: Limit location for fake CFMWS to mappable range
* cxl/EDAC: use correct format specifier for u32 value
* make cxl_bus_type constant
* Remove core/acpi.c and ACPI dependency on the core for extended linear cache size

## v6.17 merge window pending review
* Type2 device support (Alejandro)
  - v17 going through reviews
* Add managed SOFT RESERVE resource handling (Smita)
  - Pending v5
* Enable CXL PCIe port protocol error handling and logging (Terry)
  - v10 going through reviews, v11 in the works
* Delayed dport creation (Dave)
  - v5 going through review, v6 in the works
* Introduce DEFINE_ACQUIRE() (Dan)
  - Pending v2
  - Immutable branch for definition patch on cxl git
* Initialize eiw and eig (Purva)
  - Pending v2
* Low Mem Hole (Fabio)
  - Posted CXL convention doc, going through review
  - new rev in the works
* Zen5 translate part 2 (Robert)
  - expect revs to roll out with functionality in chunks like:
    region code refactor + rework extended linear cache + zen5 code
* CXL reset support for devices. (Srirangan)
  - Pending v3
* cxl: Support Poison Inject & Clear by Region Offset (Alison)
  - Pending v3 w Jonathans feedback, but more v2 comments welcome
* Allow 6 & 12 way regions on 3-way HB interleave (Alison)
  - Pending v3
* (RFC) Translate DPA->HPA in unaligned MOD3 regions (Alison)
  - Pending v1
* Make ELOG and GHES log and trace consistently (Fabio)
  - Pending v5 with updates per Jonathans review

## v6.18 and beyond
* CXL Nvdimm labels (Neeraj)
- RFC going through reviews

* Hotness Driver (Jonathan)
not revisited since last meeting - need to repost with cleaner solution for register mapping in core driver.  CHMU is the first regloc addressed thing that has hugely variable size so need to go poke inside to find out how big it is.

* non-x86 cache flushing ("wbinv") (Jonathan)
Cache flushing for non x86.  Descended into a discussion of problems with use of WBINVD on x86 so little useful discussion of what the set actually does. Some minor issues so I'll do a v3 late this week (seems unlikely to make 6.17!) Review welcome.

* DCD (Ira)
  - Anything new since June?

* vfio-cxl type 2 (Zhi)
  - Still pending v2 RFC

# June 2025
* Opens
* cxl-cli
* QEMU
* v6.16 rc fixes
* v6.17 merge window
* v6.18 and beyond

## Opens
* CXL device life time (Dan)
  * <recorded video>

* John Groves -- firmware download/activate issue
  * Can't (may not) complete within 2sec timeout - want to run as background cmd
  * revive background abort cmd patch?
    * 10 sec is upper bound for what they need
    * If abort - what state does that leave the card
      * unknown
    * device is still working during download/flash
    * orig patch was for user space cmds -> was racy
    * this use case might be ok
  * mainly need to ensure that the state returned is correct
  * Spec says one can return background command started - for firmware activate
  * can't support background in general.  lose communication
  * This is a true background operation
  * hardware with abort does not need a timeout but need one if the hardware does not support abort
  * could fw activate be a state to poll?
    * would require a new opcode
  * abort seems messy
  * you still need to reset so no need to abort just wait and reset
  * there is at least 1 cmd which polls to avoid background abort
  * there is no reason to abort until another command comes
  * this is user triggered
  * Jonathan handle this with Davidlohr because the device supports abort
    * New device would need a new mechanism - like sanitize
  * Dan prefers a forground operation which just polls for completion
    * would need a new status poll (not generic background)
  * Alternate we extend the timeout for firmware update to 1 min (eternity...)
  * wait in the shutdown flow?
  * Patch set comming w/ Davidlohr's set (regiggered)


## cxl-cli
* v82 release expected only include what is in pending today.[1]
  Any patches left out, are either pending an update or lack Reviewed-by tag.
  [1] https://github.com/pmem/ndctl/commits/pending


## QEMU
* FMAPI related stuff going on
  * FMAPI over USB working
  * should be easier to test with this
* phys switch port control stuff
* CHMU is up on gitlab
  * nearly feature complete
* some arm support
  * reference machine model but machine may not be maintained
  * feedback from arm/qemu maintainers needed


## v6.16 rc fixes (Applied to cxl/fixes)
* fix return value in cxlctl_validate_set_features()

## v6.17 merge window
### cxl/next applied
* Documentation/driver-api/cxl: Introduce conventions.rst
* Documentation: cxl: fix typos and improve clarity in memory-devices.rst
* cxl/pci: Replace mutex_lock_io() w mutex_lock() for mailbox access
* cxl_test: Limit location for fake CFMWS to mappable range
* Fix the min_scrub_cycle of a region miscalculation
  * why not 6.16?
  * obscure but is a bug
  * will move to 6.16

### cxl/next targets
* Type2 device support (Alejandro)
  - Pending v17
  - 2 issues
    - conflicts Dan pointed out
    - problems with accelerators call with objects which are not there
  - pio buffers are in CXL - lower latency
  - could there be a call back to say the mem device is comming down out from under the accelerator?
    - where could this come from?  perhaps cxl module removal?
    - link go down?
  - talk offline with Dan
  - make gross/violent but safe then clean up later
* Add managed SOFT RESERVE resource handling (Smita)
  - Pending v5
* Enable CXL PCIe port protocol error handling and logging (Terry)
  - Pending v10 - end of the week...
    - will revisit locks/reference counts
  - Will need to get new Bjorn tags.
* Delayed port enumeration (Dave)
  - Pending v4
  - Need to consider Robert's request of providing dport port_num via sysfs
    - What is the reason to require this?
      - very hard to debug without this
      - can't we export hardware ID?
        - can't because they are not struct device...
        - make them devices?
        - surface on PCI device?  (wrong pci device)
      - allocate the dports at the time we are numbering the ports
        - Dave will revisit this
* Remove core/acpi.c (Dave)
  - Pending v2
  - v3 posted - please review
* Introduce DEFINE_ACQUIRE() (Dan)
  - Going through discussions
  - Pending v2?
  - v2 will come with Peter Z's suggestions
* Using full data transfer only when offset is 0 (Ming)
  - Waiting on Jonathan to hear back from consortium on spec language interpretation
  - Jonathan will have a look
  - John G to look too
* Initialize eiw and eig (Purva)
  - Pending v2
* Low Mem Hole (Fabio)
  - Creating CXL convention doc
* Zen5 translate part 2 (Robert)
  - pending next rev?
  - ECN?
  - trying to combine with extended linear caching code already upstream
  - remove platform specific changes - make more generic
* CXL reset support for devices. (Srirangan)
  - Pending v3
* Allow 6 & 12 way regions on 3-way HB interleave (Alison)
  - Pending v3
* (RFC) Translate DPA->HPA in unaligned MOD3 regions (Alison)
  - Needs review and will need an ECN or the like also.

## v6.18 and beyond
* DCD (Ira)
  - v9 posted, still waiting for a use case
  - Jonathan - patch set still applies
  - Dan's apetite for having a sparse device dax is limited
    - don't want this to become another 'hugetlbfs'
  - have another call outside the colab meeting
* vfio-cxl type 2 (Zhi)
* Hotness Driver (Jonathan)
  - split the work...
* non-x86 cache flushing ("wbinv") (Jonathan)
  - don't have a user space ABI so use a kref...


# May 2025
* Opens
* cxl-cli
* QEMU
* v6.15 rc fixes
* v6.16 merge window
* v6.17 and beyond

## Opens
RobertR: ECN update wrt addr trans series? Can't talk confidential
side of proposal. Linux side - file code first ECNs (like ACPI) what
we want FW/BIOS to provide. Doing similar for mem hole problem.
Code first means Linux writes rules Linux needs added to CXL spec.
ACPI ECN examples, ACPI0017, extended linear cache.
By starting discussion in open, on Linux mailing list, not encumbered
for consortium confidentiality.

RobertR: patches address AMD specific addr trans, do we have other
users?  Should we be pushing a generic solution now? Ans: stay
specific now.

FanN: Issue (device probe) using DCD patch set. Has worked around it. 
FanN to post on cxl mailing list.

DanW: cxl reset - does use case include issuing a reset from userspace?


## cxl-cli / user tools
Collecting patches for a v82 release at EOQ 2, align w kernel 6.15.
* ndctl: Add support and test for CXL Features support (DaveJ)
  - Needs review tags
* ndctl: Introduce sanitize-memdev functionality (DavidLohr)
  - David pinging user who asked about in earlier this year
* ndctl: Add inject-error command (Ben)
  - ? Pending an update from Ben considering Junhyeok prior set ?
* ndctl: Dynamic Capacity additions for cxl-cli (Ira)
  - Deferred but not forgotten

## QEMU
Jonathan's Discord Update (He's enjoying fine food in Lisbon)
* Most of left over stuff that was queued for 10.0 is now queued by MST.
  One patch dropped as compile issue.
* Tcg bug introduced in some tlb cleanup work. Affecting code running from
  cxl mem and some other cases.
* Arm support v13 posted.
* Dcd fmapi updated series on list (Jonathan hasn't looked at yet).

## v6.15 RC fixes
* RC4 PR done
* No more fixes PR unless extremely urgent.

## v6.16 merge window - queued
* Remove always true condition for cxlctl_validate_hw_command()
* Verify CHBS length for CXL2.0
* Ignore interleave granularity when ways=1
* Address missing MODULE_DESCRIPTION warnings for cxl_test
* Cleanups and refactors part 1 for Zen5 translation support
* Cleanup debug printk for cxl_dpa_alloc()

## v6.16 merge window - considering
* type2 support (Alejandro)
  - v15 posted, v16 coming w rebase on rc4
* Boot to Bash documentation (Gregory)
  - v3 posted. Review tags please.
    Plan is to merge as is and expect incremental fixups can follow
- CXL Maturity Map update (Alison)
  - v2 posted. Review tags please.
* RAS features drivers (Shiju)
  - v4 posted, ready for merge?

## v6.17
* Using full data transfer only when offset is 0 (Ming)
  - Waiting on Jonathan to hear back from consortium on spec language interpretation
* Native port protocol error handling and logging (Terry)
  - Pending v9
    will need to get new Bjorn tags.
* Soft Reserve handling (Terry-->Smitha)
  - Pending v4
* Introduce DEFINE_ACQUIRE() (Dan)
  - Going through discussions
* Delayed port enumeration (Dave)
  - v2 posted, going through reviews
  - Can Robert check and see if that resolves his dport num issue reported
* Initialize eiw and eig (Purva)
  - Pending v2
* Low Mem Hole (Fabio)
  - Waiting on ECN to post next rev
* Zen5 translate part 2 (Robert)
  - pending next rev?
* CXL reset support for devices. (Srirangan)
  - Pending v3
* Allow 6 & 12 way regions on 3-way HB interleave (Alison)
  - Pending v3
* (RFC) Translate DPA->HPA in unaligned MOD3 regions (Alison)
  - Needs review and will need an ECN or the like also.

## v6.18 and beyond
* DCD (Ira)
  - v9 posted, still waiting for a use case
* vfio-cxl type 2 (Zhi)
* Hotness Driver (Jonathan)
* non-x86 cache flushing ("wbinv") (Jonathan)


# April 2025
* Opens
* cxl-cli
* QEMU
* v6.15 rc fixes
* v6.16 merge window
* v6.17 and beyond

## Opens
- DCD (Ira)
  * Fan tried a qemu test -- failing
    * Is using the latest stuff
    * Ira does not have a lot of time
  * Jonathan __will__ be taking this forward as a fork
    * please review it!
    * Is DAX ok?
    * why is this different than other features which have landed well ahead of hardware?
- Low Memory Hole enumeration (Fabio)
  * Robert wanted some changes (different direction)
    * more isolation within the implementation for special features
    * Address translation rework has a lot of conflicts
      * hard to follow
    * proposal to have a check if the LMH applies then use the SPA range
    * Dan is missing the conflicts -- refactoring is ok
      * LMH is a small change - different from a whole new addressing space
      * what happens when a 3rd, 4th...  etc show up?
        * don't be surprised by these things(?)
      * why does this quirk need to be delayed by larger changes?
        * some code conflicts
        * but does LMH break the new code?
    * Robert - extended linear caching is harder to abstract and LMH makes that harder
      * wants some code isolation
      * flat2lm messes this up too - LMH is yet another thing
      * is the refactoring for flat2lm done?
        * not yet
      * the refactoring should make LMH fit easier
        * makes SPA != HPA => use for LMH
      * part 2 of Roberts series would do this.
        * this has been posted. "Address translation part 1 and 2"
        * part 1 does not conflict as much
          * Could be helpful to get this landed to clear the backlog
        * part 2 mostly needs to be resolved -> hard
  * Linux has suffered from platforms taking liberties (inveted on the fly)
    * there has to be a conversation somewhere on these special configs
    * can we get some rules around these things
  * examples
    * no CFMWS for type2
    * SPA vs HPA
* John G. -- FAMfs RFC v6.14 out soon.  6.15 rework comming
* Gregory working on boot to bash stuff to put in documentation
  - need opinions on this
  - in a personal-public github


## cxl-cli / user tools
* v81 was released end of Q1.
* Collecting features for a v82 at end of Q2, aligned w 6.15.
  * ndctl: Add support and test for CXL Features support (DaveJ)
    - __Needs review__. Driver support is in.
  * ndctl: Introduce sanitize-memdev functionality (DavidLohr)
    - __Needs review__. Driver support is in.
  * ndctl: Add inject-error command (Ben)
	  - Pending an update from Ben
  * ndctl: Dynamic Capacity additions for cxl-cli (Ira)
    - Awaiting driver decision
    - might need some clean up on the base commit
    - but it is out there

## QEMU
* 10.0 out today or next week
* fairly minor features will land after that
* arm support waiting for __review__
* FM-API __review__
* FM in qemu A controlling devices in qemu B
  * RFC - test FM commands through MCTP
  * uses QMP to notify qemu B
  * MCTP messages is in shared buffer
  * Need feedback from upstream -> may need a socket vs shared buffer
  * 'whatever works' ...
  * FM in host could work -> nice to have kernel stack formulate MCTP
  * what blocks MCTP
    * open BMC is blocked by lack of tests
      * need to know what happens with malformed packets
    * long way around is to use a PCI (with a distro) -> i2c emulated device
      * could abuse this work.
  * could just use ARM for MCTP with open BMC


## v6.15 rc fixes
- Pending cxl/fixes
* GPF DVSEC fixes (Ming)

- Waiting on more review tags
* CXL Features: Address set_feature and offset flag (Ming)
  * email sent
* CXL Features: Set out_len in set_feature failure case (Ming)
* Skip Mem_En check for RCD and RCH ports (Smita)


## v6.16 merge window
- Pending cxl/next
* Ignore interleave granularity when ways=1 (Gregory)
* Verify CHBS length for CXL 2.0 (Zhijian)
* Remove always true condition for cxlctl_validate_hw_command() (DaveJ)

- Waiting on more review tags
* CXL type2 support (Alejandro)
  - Going through v13 review 
* Enable CXL PCIe port protocol error handling and logging (Terry)
  - Going through v8 review
  - working on it but ioresource is higher priority
* AMD Zen5 address translation support (Robert)
  - Going through v2 review
  - will send part 1 first and can focus on that now
* Managed SOFT RESERVE resource handling (Terry)
  - Going through v3 review
  - build bot issues v4 comming.
* Enable region creation on x86 with low memory hole (Fabio)
  - Discussion on going
  - Focus on clean ups (part 1 series) then decide on LMH
* Delay dport initialization (DaveJ)
  - Going through v1 review
* CXL reset support for devices. (Srirangan)
  - Going through v2 review
  - PCIe subsystem review
  - v3 needed but awaiting more comments prior
* Allow 6 & 12 way regions on 3-way HB interleave (Alison)
  - Pending a v2 update
* Translate DPA->HPA in unaligned MOD3 regions (Alison)
  - Needs review
  - label RFC but please review anyway
  - priority vs LMH/part 1 rework/part 2?
    - Gregory does not see anything obvious but will take a quick look
      - may be subtle interleave position issues
      - any 3-way region will be unaligned
* Update CXL maturity map. (Alison)
  - Need review?
  - the maturity map needs more review but not Alisons' patch itself
  - Please update the maturity map as part of documenting any changes one submits
* RAS features drivers
  - ACPI should land in 6.16 too
  - locking bugs -> fixed in v3 just posted


## v6.17 and beyond
* DCD support (Ira)
  - v9 posted
  - Dan's uncomfort
    - Dan did a public demo almost 2 years ago -> all dissapeared
    - device dax does not have review scalability
      - low priority for Dan
    - would like to have an end user stand up
    - what about the FM development
    - this has stopped being a priority
  - John Groves - was shown a demo - not public...  yet
    - sharable memory
    - Has a person who has been testing this
    - emulation is probably at a point that John could test this
    - AR : John look at the interface and contribute what they need with tagging to make this work for them.  With a real use case.
  - Johnathan prefered some of the older interfaces but these have all been ok
    - Also need a virtualization story
    - virtio plan is gone
      - cxl emulation is in
    - keep this alive on top of type2
    - Johnathan will continue to have a staging branch - as stated above
  - Gregory
    - is dax the right interface for virtualization
    - among us we have users
  - Yannis - Is this chicken and egg?
    - there are demos
  - Is the interface what we want to support?
    - is it sufficient?
    - Dan we know this is not a slam dunk interface
    - does it matter if dax may go away?
      - dax was advocated at LSFmm - so not going away
* vfio-cxl type 2 (Zhi)
  - next version?
* Hotness Driver (Jonathan)
  - focus is on emulation (qemu) first
* non-x86 cache flushing ("wbinv")
  - need review
  - arm focused but should work anywhere
  - used device classes (show in sysfs)
    - but no user interface now
    - could be used for specific flushes
      - various methods


# March 2025
* Opens
* cxl-cli
* QEMU
* v6.14 rc fixes
* v6.15 merge window
* v6.16 and beyond

## Opens
- Mixed granularity in x3 regions (Alison)
  - posted on the list 6&12 way regions
  - fix is limited
    - this is fine but could this be simpler by relaxing ordering constraints?
    - perhaps this makes no difference at all?
  - way back - mismatch - interleave was "backwards"
    - course vs fine
  - if there are other use cases of x3 - please review her patch
- LSFmm -- Intel attendees?
  - DCD?
  - Dan should be there
  - CXL specific session?
    - 1 hour might be better than 1/2
  - device specific stuff maybe in the hallway track?
  - Gregory to focus on external to the driver
  - General CXL discussions 80% on external with maybe 20% internal driver stuff
  - chime in on the ML on what you would like to see


## cxl-cli / user tools
* v81 is queued up with misc fixups.
* New features on the list:
  Davidlohr
  * ndctl: Introduce sanitize-memdev functionality (Davidlohr)
  Dave J.
  * fwctl changes?
  Pending an update from David
  * ndctl: Dynamic Capacity additions for cxl-cli (Ira)
	Simmering waiting for entry into cxl/next
  * ndctl: Add support and test for CXL Features support (DaveJ)
	Simmering waiting for all the pieces to come together for test and review
  * ndctl: Add inject-error command (Ben)
	Pending an update from Ben


## QEMU
Missed Michael's merge window
* queued up for next cycle
* DCD FM api stuff
  - interacts with Gregory FM stuff
  - idealy would be connected eventually
* Who is using DCD/qemu
  * Jonathan has always tested DCD with qemu
  * Terry, Adam, and John Groves all using qemu DCD
  * Adam -- DCD would be nice for compression
    - Yiannis -- some concerns due to lack of use case for DCD
  * John G. allocations from 0 within same tag - similar to storage but not exactly the same
* ARM support review needed
  - got blocked way back for device tree support - relaxed
* hotpage support
  - please chime in if you are interested

## v6.14 rc fixes
none

## 6.15 in cxl/next
Already in from last meeting:
* Add support for Global Persistent Flush (GPF)
* Cleanup of DPA partition metadata handling
* Removed unused CXL partition values
* Refactor user ioctl command path from mds to cxl_mailbox
* Add logging support for CXL CPER endpoint and port protocol errors
* Remove redundant gp_port init

Newly added:
* Cleanup of gotos using guard() series
* Validation of CXL device serial number
* CXL ABI documentation update/fixups
* CXL Features support (First part of CXL FWCTL support)
  - FWCTL specific CXL bits will be pushed by Jason G.
* Additional support for dirty shutdowns
* Extended Linear Cache enumeration and RAS support
* Last 2 patches from Smita for firmware first error logging
* cxl_test to support 3-way capable CFMWS
* Documentation fix to remove "mixed mode"

## 6.15 merge window considerations
* 6.15 merge window closed for large series.
* May still take small changes or fixes that are urgent.

## v6.16 and beyond
* cxl: Add address translation support and enable AMD Zen5 platforms (Robert)
  - v4 of part1 in review
  - v3 of part2 pending?
* Update soft reserved resource handling (Nathan -> Terry)
  - v3 pending
  - next version this week
* CXL PCIe port protocol error handling and logging (Terry)
  - v8 pending?
  - tomorrow...
* Support CXL memory RAS features - EDAC (Shiju)
  - pending v2?  (AKA v24 ...  ;-)
  - core support landed
    - yay!  can do this now
* Support background operation abort requests (Davidlohr)
  - pending v2?
* Enable Region creation on x86 with Low Mem Hole (Fabio)
  - v3 posted, under review. Would like this queued for cxl/next after merge 
    window.
* Type2 device support (Alejandro)
  - v11 posted. Under review. Would like this queued for cxl/next next merge 
    window.
* Rest of DCD series (Ira)
  - Pending type2 acceptance
  - look for another version after the merge window
    - will be watered down from previous version
  - need use cases beyond the spec
  - The cost of not merging this is that nothing is being built above it
  - Several CPU vendors talking about hot add/remove
    - is DCD a way to get around this?
  - flush cost limits things
  - a motivation is to decouple decoder programming from on/off lining memory
  - entire provisioning mechanism is DCD with CXL 3 - endpoint
  - use case order is important as well
  - Linus/mm folks won't care -- But Dan wants the folks shipping products to stand up.
* Allow 6 & 12 way regions on 3-way HB interleaves (Alison)
  - Pending v3?
  - already discussed above (in opens)
* Translate DPA->HPA in unaligned MOD3 regions (Alison)
  - v1 needs review
* cxl: factor out cxl_await_range_active() and cxl_media_ready() (Zhi)
  - Pending next rev?
* Add cxl reset support (Srirangan)
  - Pending review
  - PCIe folks looking at this...
* Cleanup add_port_attach_ep() "cleanup" confusion (Dan)
  - Dan needs to review

## RFC
* vfio-cxl type 2 (Zhi)
  - Pending next rev
  - Zhi could not make the call
* Hotness Driver (Jonathan)
  - lots to do here
  - Combining all hotness features CXL and beyond - Discuss at LSFmm
    - DAMON...  NO... ?
      - one options on the table
    - save it for LSFmm

* boot to bash
  - Gregory's documentation journey
  - kernel docs?
    - AI will pick it up from there.
  - CEDT recipes?
  - good feedback on this
  - LSFmm session will focus on how Linux expects things to be configured
    - memory blocks vs region alignment -> lose memory
    - Theory vs how Linux really works
  - All of the complication comes from BIOS and OS interactions after BIOS sets things up
    - Need OS first!!!
    - ACPI tables need to be correct.
    - <sigh>  backwards compatibility...
  - please tear the docs appart if you think it is wrong

* FAMfs is running under FUSE!
  - patch to libfuse
  - may have a branch before LSF


# February 2025
* Opens
* cxl-cli
* QEMU
* v6.14 rc fixes
* v6.15 merge window
* v6.16 and beyond

## Opens
* Bueler?  <none>

## cxl-cli / user tools

* v81 is open with misc fixups
* Need review on build and coverity fixups on list
* New features in review:
  * ndctl: Introduce sanitize-memdev functionality (Davidlohr)
	https://lore.kernel.org/linux-cxl/20240928211643.140264-1-dave@stgolabs.net/
  * ndctl: Dynamic Capacity additions for cxl-cli (Ira)
	https://lore.kernel.org/nvdimm/20241214-dcd-region2-v4-0-36550a97f8e2@intel.com
  * ndctl: Add support and test for CXL Features support (DaveJ)
	https://lore.kernel.org/linux-cxl/20250207234718.2387622-1-dave.jiang@intel.com/
  * ndctl: Add inject-error command (Ben)
	https://lore.kernel.org/nvdimm/20250108215749.181852-1-Benjamin.Cheatham@amd.com/

## QEMU
* inside merge window
* Fujitsu clean up
* ARM virt support reposted
* 2 samsung series'
* Hotlist monitoring


## v6.14 rc fixes
none

## 6.15 in cxl/next
* Add support for Global Persistent Flush (GPF)
* Cleanup of DPA partition metadata handling
* Removed unused CXL partition values
* Refactor user ioctl command path from mds to cxl_mailbox
* Add logging support for CXL CPER endpoint and port protocol errors
* Remove redundant gp_port init

## 6.15 merge window
* Rest of DCD series (Ira)
  - pending v9
  - much discussion on actual use cases
  - AI: Ira to schedule another call between Dan, John, Jonathan and Ira...
* Support background operation abort requests (Davidlohr)
  - Pending v2
  - will rebase and send soon
* CXL PCIe port protocol error handling and logging (Terry)
  - v7 posted, review on going
  - Some devices/drivers have been happily CXL-unaware (prtdrv)
    - should PCI subsystem throw errors to 'cxl land'?
    - CXL system must be loaded for processing these errors
    - Alternate: make PCI system more cxl aware
    - new file is ok
    - 2 fifos CPER/OS first
    - fifo overflows if cxl is not loaded (user 'asked for it')
  - mapping between PCI/CXL device
  - AER -> fifo -> wq -> pciaer -> aer src info???
  - AER -> fifo -> cxl core?
  - AER statistics (CXL counters?)
    - Jonathan to post reference
* Type2 device support (Alejandro)
  - v10 posted, review on going
  - memdev state vs device state
  - need to have Alejandro to discuss further
* Trace FW-First CXL Protocol Errors (Smita)
  - 1-4 in cxl/next. 5&6 needs more work
  - Q: looks like this needs to be in cxl core?
    - yes it is not an endpoint but a port object
* cxl: Add address translation support and enable AMD Zen5 platforms (Robert)
  - Part 1&2 v2 posted
  - Review on going
  - Reference Low memory hole: was generic to 'some platform may do this'
  - specific AMD file?  can this be more generic?
    - has anyone else done this?  ...  no
    - specification help?
* Update soft reserved resource handling (Nathan, Alison)
  - v2 is posted. Review on going?
  - Hildenbrand(sp?) had comments
* Introduce generic EDAC RAS control feature driver (Shiju)
  - v19 posted. Review ongoing?
  - v19 discussion : Boris is unconvinced about the API
    - reverse engineer tracepoints?
    - marshal into ioctl
    - marshal into tracepoint (format different)
    - use sysfs
    - lockdown kernels disable debugfs
    - Must be in EDAC - Boris
  - v20 posted
    - Dan to look at CXL bits
      - online repair - must see an error 'this boot'
    - call for memory device manf. to look at the API and weigh in
    - can parameters just fine
* FWCTL CXL (Dave)
  - pending v6
* Add exclusive caching enumeration and RAS support (Dave)
  - v3 posted, minor changes requested from Ming. Need Dan's review
* Enable Region creation on x86 with Low Mem Hole (Fabio)
  - v2 posted, review ongoing.
  - some assertion this should wait for Roberts stuff?
  - Can we apply this?
  - this could affect Roberts patches with a small conflict - Gregory
* cxl: factor out cxl_await_range_active() and cxl_media_ready() (Zhi)
  - pending v3?
  - will respin
* vfio-cxl type 2 (Zhi)
  - will rebase on type 2 v10+
* Use guard() instead of rwsem locking cleanup series (Ming)
  - v2 review on going
  - should be simple; get reveiwng folks!
* cxl/pmem: debug invalid serial number data (Yuquan)
  - v3 review on going
* Add cxl reset support (Srirangan)
  - review on going
* Dirty shutdown followups (Davidlohr)
* Cleanup add_port_attach_ep() "cleanup" confusion (Dan)
  - pending v3?
  - forgotten...  will be remembered...

## 6.16 and beyond
* Hotness driver (Jonathan)
  - might need a sub-call on this; Jonathan to schedule
* vfio-cxl? (Zhi)
  - see above...


# January 2025
* Opens
* cxl-cli
* QEMU
* v6.14 merge window
* v6.14 rc fixes
* v6.15 merge window
* v6.16 and beyond

## Opens
* 1.5 hour meeting?
* Feature velocity is slowing
  * Upstream support needs to land well ahead of distro acceptance
  * Some vendors may not release hardware without ecosystem support
    * lack of hardware does not mean a feature is not important
    * DCD is important but type 2 seems to be more important because devices are out and real
      * The core had an issue which probably should have been cleaned up a while ago
      * both DCD and type 2 were trying to not 'disrupt' the status quo
    * Generally clean up first is good
    * In this case there are other issues with both sets so it might be fine...  this time
  * Cross-subsystem ties?
  * Jonathan would like to see at least one of these features queued for cxl-next
  * More reviews!
    * CXL will now be 'leaking' out into the rest of the kernel.
    * Use cases may need to make CXL core changes...  without redoing the entire thing.
  * type 2 is higher prioity than error handling.  may folks are doing FW first.
    * there are some FW first patches on the tail end of the port error handling series.
      * Maybe those should come first or as a separate patch set?
      * AFAWK they don't conflict with type 2
  * DCD can go in with device dax.  memfd is a future question.  should device dax move toward memfd?
    * famfs needs device dax.  without memfd changes it can't replace device dax.
    * memfd is currently geared toward anonymous memory and device dax provides better super block support
    * also tagging support in memfd would also be a bigger change
    * memfd support may be growing some persistence so there may be conflicts in the support and a decision may need to be made.
      * public?  guest memfd call is public
      * Gowans is working on this
    * CC can't consume device dax.  which also pushes toward memfd
      * How would CC handle shared memory?  -- can't be anonymous
      * shared confidential is down the road -- we are not ready for it
* ratelimit AER
  * https://lore.kernel.org/linux-pci/cover.1736341506.git.karolina.stolarek@oracle.com/
  * https://lore.kernel.org/linux-pci/20250115074301.3514927-1-pandoh@google.com/
  * Pradeep to check with Terry
* CXL reset
  * patch set should apply to all devices
  * Don't the SBR patches already do this?
    * This would destroy the regions
    * We think so
    * If this is adding a new reset to that then we should be ok
  * CXL reset is different
    * It does work through sysfs
    * expected use case would be to go through type 2 driver and it could ensure memory flushes
  * Does this need a new version?
    * Still RFC
  

## cxl-cli / user tools
* v81 is open with misc fixups and unit test updates at the moment
* New features need review:
  - DCD needs review (Ira)
    https://lore.kernel.org/nvdimm/20241115-dcd-region2-v3-0-585d480ccdab@intel.com/
  - Sanitize memdev needs review (Davidlohr)
    https://lore.kernel.org/linux-cxl/20240928211643.140264-1-dave@stgolabs.net/
  - Inject error (Ben)
    https://lore.kernel.org/nvdimm/20250108215749.181852-1-Benjamin.Cheatham@amd.com/

## QEMU
* one fix around MSI
* clean up some error paths
* new staging tree should be out in a day or 2
  * hot miss -- HMU
    * roughly speaking one can run a real work load and get real data
    * 10% speed (4min to boot)
    * infinate counters (no one will build this) -- could add knob to change counters
    * framework is in place
      * if other HW vendors like to upstream 'real' behavior
      * would be nice to have a range of implementations


## In v6.14 merge window
* ACPI/HMAT: Move HMAT messages to pr_debug()
* cxl/pci: Add CXL Type 1/2 support to cxl_dvsec_rr_decode()
* CXL events updates for spec r3.1 (series)

cxl-next may still consider the following. Would like to close cxl-next by Wed/Thurs.
* cxl/pci: Support Global Persistent Flush (GPF)
  - Need review tags
* DPA partition meta data cleanup
  - Need v2 and review tags

## v6.14 rc fixes
* None so far

## v6.15 merge window
* Rest of DCD series (Ira)
  - Pending v8
  - pending on DPA partition cleanups from Dan
  - week?
* Support background operation abort requests (Davidlohr)
  - Pending v2
  - user space would abort previous operation
  - v2 on the way
* CXL PCIe port protocol error handling and logging (Terry)
  - v5 posted, review on going?
  - yea
* Type2 device support (Alejandro)
  - v9 posted, need review
  - Also pending on DPA partition cleanups from Dan
  - is there a way to check if an allocation came from devm?
    - xfc is a network driver
    - put cxl side of the driver in drivers/cxl
    - Alejandro believes he has a solution
    - VFIO also has this problem
      - export a new function so the VFIO can do the release
    - convert some core calls to non-devm
  - patch set has been tested with 2 different drivers and AMD
    - should be very stable
    - cxl-test has not been run though
    - but also can't break the cxl-test build
    - should also have a basic cxl-smoke test-test
    - would love that every feature has a new cxl-test but not always required
* Trace FW-First CXL Protocol Errors (Smita)
  - v5 posted. Is linux-efi picking up the series?
* cxl: Add address translation support and enable AMD Zen5 platforms
  - Review on going
* Add device reporting poison handler (Shiyang)
  - Will there be v5? No movement since last September
  - rasdaemon folks are engaged
  - rasdaemon will find the tracepoints
  - what about corrected errors.  would need to soft offline
    - who does that?  don't know yet.
* Update soft reserved resource handling (Nathan, Alison)
  - v2 is posted. Review on going?
* Introduce generic EDAC RAS control feature driver (Shiju)
  - v18 posted. Review ongoing?
  - v19 comming
  - complexity around the interface caught on merge
    - specifically memory sparing -- because DPA is not stable
    - add PoC in user space to show the usage of the API
    - need vendors to step up to show this is not a single vendor solution
    - does this just belong more on the CXL side vs EDAC
      - but Boris wanted it in EDAC -- for unified interfaces
    - maybe write a whitepaper to help explain -- it is in the documentation
  - when is it safe to use the interfaces?  ie after a boot?
    - safty rules vary a bit
    - error record must corespond to the current boot
    - can a device really do this...  'atomic swap'
    - soft-hibernate idea
    - just document it -- device is to take care of it
    - feature query
  - xarray will carry on forever ...  should not see that many errors
  - PPR will have separate support
* FWCTL CXL (Dave)
  - Almost done with cxl cli support and cxl unit test for using ioctls.
    v1 will be posted once that is done.
* Add exclusive caching enumeration and RAS support (Dave)
  - v3 posted, minor changes requested from Ming. Need Dan's review
  - all tags from Jonathan
* Enable Region creation on x86 with Low Mem Hole (Fabio)
  - v2 posted, review ongoing.
  - Need to sync with Robert's address translation series?
  - not sure how to do what Robert suggests
* cxl: factor out cxl_await_range_active() and cxl_media_ready() (Zhi)
  - v2 posted, Need more review tags
* Cleanup add_port_attach_ep() "cleanup" confusion (Dan)
  - pending v3?

## 6.16 and beyond
* Hotness driver (Jonathan)
  - Updates?
* vfio-cxl?
  - Any new updates?

## Admin Issues
* Anyone want to host the next meeting?

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
