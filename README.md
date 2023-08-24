# ndctl

Utility library for managing the libnvdimm (non-volatile memory device)
sub-system in the Linux kernel
  
<a href="https://repology.org/project/ndctl/versions">
    <img src="https://repology.org/badge/vertical-allrepos/ndctl.svg" alt="Packaging status" align="right">
</a>

Build
=====

```
meson setup build;
meson compile -C build;
```

Optionally, to install:

```
meson install -C build
```

There are a number of packages required for the build steps that may not
be installed by default.   For information about the required packages,
see the "BuildRequires:" lines in ndctl.spec.in.

https://github.com/pmem/ndctl/blob/main/ndctl.spec.in

Documentation
=============
See the latest documentation for the NVDIMM kernel sub-system here:
  
https://www.kernel.org/doc/html/latest/driver-api/nvdimm/index.html

A getting started guide is also available on the kernel.org nvdimm wiki:

https://nvdimm.wiki.kernel.org/start

Unit Tests
==========
The unit tests run by `meson test` require the nfit_test.ko module to be
loaded.  To build and install nfit_test.ko:

1. Obtain the kernel source.  For example,  
   `git clone -b libnvdimm-for-next git://git.kernel.org/pub/scm/linux/kernel/git/nvdimm/nvdimm.git`  

1. Skip to step 3 if the kernel version is >= v4.8.  Otherwise, for
   kernel versions < v4.8, configure the kernel to make some memory
   available to CMA (contiguous memory allocator). This will be used to
   emulate DAX.  
   ```
   CONFIG_DMA_CMA=y
   CONFIG_CMA_SIZE_MBYTES=200
   ```
   **or**  
   `cma=200M` on the kernel command line.  

1. Compile the libnvdimm sub-system as a module, make sure "zone device"
   memory is enabled, and enable the btt, pfn, and dax features of the
   sub-system:  

   ```
   CONFIG_X86_PMEM_LEGACY=m
   CONFIG_ZONE_DEVICE=y
   CONFIG_LIBNVDIMM=m
   CONFIG_BLK_DEV_PMEM=m
   CONFIG_BTT=y
   CONFIG_NVDIMM_PFN=y
   CONFIG_NVDIMM_DAX=y
   CONFIG_DEV_DAX_PMEM=m
   CONFIG_ENCRYPTED_KEYS=y
   CONFIG_NVDIMM_SECURITY_TEST=y
   CONFIG_STRICT_DEVMEM=y
   CONFIG_IO_STRICT_DEVMEM=y
   ```

1. Build and install the unit test enabled libnvdimm modules in the
   following order.  The unit test modules need to be in place prior to
   the `depmod` that runs during the final `modules_install`  

   ```
   make M=tools/testing/nvdimm
   sudo make M=tools/testing/nvdimm modules_install
   sudo make modules_install
   ```

1. CXL test

   The unit tests will also run CXL tests by default. In order to make these
   work, we need to install the cxl_test.ko as well.

   Obtain the CXL kernel source(optional).  For example,
   `git clone -b pending git://git.kernel.org/pub/scm/linux/kernel/git/cxl/cxl.git`

   Enable CXL-related kernel configuration options.
   ```
   CONFIG_CXL_BUS=m
   CONFIG_CXL_PCI=m
   CONFIG_CXL_ACPI=m
   CONFIG_CXL_PMEM=m
   CONFIG_CXL_MEM=m
   CONFIG_CXL_PORT=m
   CONFIG_CXL_REGION=y
   CONFIG_CXL_REGION_INVALIDATION_TEST=y
   CONFIG_DEV_DAX_CXL=m
   ```
1. Install cxl_test and related mock modules.
   ```
   make M=tools/testing/cxl
   sudo make M=tools/testing/cxl modules_install
   sudo make modules_install
   ```
1. Now run `meson test -C build` in the ndctl source directory, or `ndctl test`,
   if ndctl was built with `-Dtest=enabled` as a configuration option to meson.

1. To run the 'destructive' set of tests that may clobber existing pmem
   configurations and data, configure meson with the destructive option after the
   `meson setup` step:

   ```
   meson configure -Dtest=enabled -Ddestructive=enabled build;
   ```

Troubleshooting
===============

The unit tests will validate that the environment is set up correctly
before they try to run. If the platform is misconfigured, i.e. the unit
test modules are not available, or the test versions of the modules are
superseded by the "in-tree/production" version of the modules `meson
test` will skip tests and report a message like the following in
`build/meson-logs/testlog.txt`

```
SKIP: libndctl
==============
test/init: nfit_test_init: nfit.ko: appears to be production version: /lib/modules/4.8.8-200.fc24.x86_64/kernel/drivers/acpi/nfit/nfit.ko.xz
__ndctl_test_skip: explicit skip test_libndctl:2684
nfit_test unavailable skipping tests
```

If the unit test modules are indeed available in the modules 'extra'
directory the default depmod policy can be overridden by adding a file
to /etc/depmod.d with the following contents:  

```
override nfit * extra
override device_dax * extra
override dax_pmem * extra
override dax_pmem_core * extra
override dax_pmem_compat * extra
override libnvdimm * extra
override nd_btt * extra
override nd_e820 * extra
override nd_pmem * extra
```

The nfit_test module emulates pmem with memory allocated via vmalloc().
One of the side effects is that this breaks 'physically contiguous'
assumptions in the driver. Use the '--align=4K option to 'ndctl
create-namespace' to avoid these corner case scenarios.
