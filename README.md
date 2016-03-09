# ndctl
Utility library for managing the libnvdimm (non-volatile memory device)
sub-system in the Linux kernel
  

Build
=====
`./autogen.sh`  
`./configure --enable-local`  
`make`  
`make check`  
`sudo make install`  

Documentation
=============
See the latest documentation for the NVDIMM kernel sub-system here:
  
https://git.kernel.org/cgit/linux/kernel/git/nvdimm/nvdimm.git/tree/Documentation/nvdimm/nvdimm.txt?h=libnvdimm-for-next

Unit Tests
==========
The unit tests run by `make check` require the nfit_test.ko module to be
loaded.  To build and install nfit_test.ko:

1. Obtain the kernel source.  For example,  
`git clone -b libnvdimm-for-next
git://git.kernel.org/pub/scm/linux/kernel/git/nvdimm/nvdimm.git`  

2. Configure the kernel to make some memory available to CMA (contiguous
   memory allocator).
This will be used to emulate DAX.  
`CONFIG_DMA_CMA=y`  
`CONFIG_CMA_SIZE_MBYTES=200`  
**or**  
`cma=200M` on the kernel command line.  

3. Compile all components of the libnvdimm sub-system as modules:  
`CONFIG_LIBNVDIMM=m`  
`CONFIG_BLK_DEV_PMEM=m`  
`CONFIG_ND_BLK=m`  
`CONFIG_ND_BTT=m`  

4. Build and install the unit test enabled libnvdimm modules in the
   following order.  The unit test modules need to be in place prior to
   the `depmod` that runs during the final `modules_install`  
`make M=tools/testing/nvdimm/`  
`sudo make M=tools/testing/nvdimm/ modules_install`  
`sudo make modules_install`

5. Now run `make check` in the ndctl source directory, or `ndctl test`,
   if ndctl was built with `--enable-test`.

