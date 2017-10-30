include $(top_srcdir)/Makefile.am.in

TESTS =\
	libndctl \
	dsm-fail \
	dpa-alloc \
	parent-uuid \
	multi-pmem \
	create.sh \
	clear.sh \
	dax-errors.sh \
	daxdev-errors.sh \
	multi-dax.sh \
	btt-check.sh \
	label-compat.sh \
	blk-exhaust.sh \
	sector-mode.sh \
	inject-error.sh \
	btt-errors.sh

check_PROGRAMS =\
	libndctl \
	dsm-fail \
	dpa-alloc \
	parent-uuid \
	multi-pmem \
	dax-errors \
	daxdev-errors

if ENABLE_DESTRUCTIVE
TESTS +=\
	blk-ns \
	pmem-ns \
	dax-dev \
	dax.sh \
	device-dax \
	device-dax-fio.sh \
	mmap.sh

check_PROGRAMS +=\
	blk-ns \
	pmem-ns \
	dax-dev \
	dax-pmd \
	device-dax \
	mmap
endif

LIBNDCTL_LIB =\
       ../ndctl/lib/libndctl.la \
       ../daxctl/lib/libdaxctl.la

testcore =\
	core.c \
	../util/log.c \
	../util/sysfs.c

libndctl_SOURCES = libndctl.c $(testcore)
libndctl_LDADD = $(LIBNDCTL_LIB) $(UUID_LIBS) $(KMOD_LIBS)

dsm_fail_SOURCES =\
	dsm-fail.c \
	$(testcore)

dsm_fail_LDADD = $(LIBNDCTL_LIB) $(KMOD_LIBS)

blk_ns_SOURCES = blk_namespaces.c $(testcore)
blk_ns_LDADD = $(LIBNDCTL_LIB) $(KMOD_LIBS)

pmem_ns_SOURCES = pmem_namespaces.c $(testcore)
pmem_ns_LDADD = $(LIBNDCTL_LIB) $(KMOD_LIBS)

dpa_alloc_SOURCES = dpa-alloc.c $(testcore)
dpa_alloc_LDADD = $(LIBNDCTL_LIB) $(UUID_LIBS) $(KMOD_LIBS)

parent_uuid_SOURCES = parent-uuid.c $(testcore)
parent_uuid_LDADD = $(LIBNDCTL_LIB) $(UUID_LIBS) $(KMOD_LIBS)

dax_dev_SOURCES = dax-dev.c $(testcore)
dax_dev_LDADD = $(LIBNDCTL_LIB) $(KMOD_LIBS)

dax_pmd_SOURCES = dax-pmd.c
mmap_SOURCES = mmap.c
dax_errors_SOURCES = dax-errors.c
daxdev_errors_SOURCES = daxdev-errors.c \
			../util/log.c \
			../util/sysfs.c
daxdev_errors_LDADD = $(LIBNDCTL_LIB)
device_dax_SOURCES = \
		device-dax.c \
		dax-dev.c \
		dax-pmd.c \
		$(testcore) \
		../ndctl/namespace.c \
		../ndctl/check.c \
		../util/json.c
device_dax_LDADD = \
		$(LIBNDCTL_LIB) \
		$(KMOD_LIBS) \
		$(JSON_LIBS) \
		../libutil.a

multi_pmem_SOURCES = \
		multi-pmem.c \
		$(testcore) \
		../ndctl/namespace.c \
		../ndctl/check.c \
		../util/json.c
multi_pmem_LDADD = \
		$(LIBNDCTL_LIB) \
		$(JSON_LIBS) \
		$(UUID_LIBS) \
		$(KMOD_LIBS) \
		../libutil.a
