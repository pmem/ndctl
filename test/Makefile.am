include $(top_srcdir)/Makefile.am.in

TESTS =\
	libndctl \
	dpa-alloc \
	parent-uuid \
	create.sh \
	clear.sh \
	dax-errors.sh

check_PROGRAMS =\
	libndctl \
	dpa-alloc \
	parent-uuid \
	dax-errors

if ENABLE_DESTRUCTIVE
TESTS +=\
	blk-ns \
	pmem-ns \
	dax-dev \
	dax.sh \
	mmap.sh

check_PROGRAMS +=\
	blk-ns \
	pmem-ns \
	dax-dev \
	dax-pmd \
	mmap
endif

LIBNDCTL_LIB =\
       ../ndctl/lib/libndctl.la \
       ../daxctl/lib/libdaxctl.la

libndctl_SOURCES = libndctl.c core.c
libndctl_LDADD = $(LIBNDCTL_LIB) $(UUID_LIBS) $(KMOD_LIBS)

blk_ns_SOURCES = blk_namespaces.c core.c
blk_ns_LDADD = $(LIBNDCTL_LIB) $(KMOD_LIBS)

pmem_ns_SOURCES = pmem_namespaces.c core.c
pmem_ns_LDADD = $(LIBNDCTL_LIB) $(KMOD_LIBS)

dpa_alloc_SOURCES = dpa-alloc.c core.c
dpa_alloc_LDADD = $(LIBNDCTL_LIB) $(UUID_LIBS) $(KMOD_LIBS)

parent_uuid_SOURCES = parent-uuid.c core.c
parent_uuid_LDADD = $(LIBNDCTL_LIB) $(UUID_LIBS) $(KMOD_LIBS)

dax_dev_SOURCES = dax-dev.c core.c
dax_dev_LDADD = $(LIBNDCTL_LIB)

dax_pmd_SOURCES = dax-pmd.c
mmap_SOURCES = mmap.c
dax_errors_SOURCES = dax-errors.c
