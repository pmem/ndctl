include $(top_srcdir)/Makefile.am.in

bin_PROGRAMS = ndctl

ndctl_SOURCES = ndctl.c \
		builtin-create-nfit.c \
		builtin-xaction-namespace.c \
		builtin-xable-region.c \
		builtin-dimm.c \
		builtin-list.c \
		builtin-test.c \
		builtin-help.c \
		util/json.c

if ENABLE_SMART
ndctl_SOURCES += util/json-smart.c
endif

if ENABLE_DESTRUCTIVE
ndctl_SOURCES += ../test/blk_namespaces.c \
		 ../test/pmem_namespaces.c
ndctl_SOURCES += builtin-bat.c
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
		 ../util/log.c \
		 ../util/sysfs.c \
		 ../test/dpa-alloc.c \
		 ../test/parent-uuid.c \
		 ../test/multi-pmem.c \
		 ../test/core.c
endif
