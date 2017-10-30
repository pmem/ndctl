include $(top_srcdir)/Makefile.am.in

bin_PROGRAMS = ndctl

ndctl_SOURCES = ndctl.c \
		create-nfit.c \
		namespace.c \
		check.c \
		region.c \
		dimm.c \
		 ../util/log.c \
		list.c \
		test.c \
		../util/json.c \
		inject-error.c

if ENABLE_SMART
ndctl_SOURCES += util/json-smart.c
endif

if ENABLE_DESTRUCTIVE
ndctl_SOURCES += ../test/blk_namespaces.c \
		 ../test/pmem_namespaces.c
ndctl_SOURCES += bat.c
endif

ndctl_LDADD =\
	lib/libndctl.la \
	../daxctl/lib/libdaxctl.la \
	../libutil.a \
	$(UUID_LIBS) \
	$(KMOD_LIBS) \
	$(JSON_LIBS)

if ENABLE_TEST
ndctl_SOURCES += ../test/libndctl.c \
		 ../test/dsm-fail.c \
		 ../util/sysfs.c \
		 ../test/dpa-alloc.c \
		 ../test/parent-uuid.c \
		 ../test/multi-pmem.c \
		 ../test/core.c
endif
