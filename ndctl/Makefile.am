include $(top_srcdir)/Makefile.am.in

BUILT_SOURCES = libndctl.h
lib/libndctl.h: libndctl.h.in
	touch $(top_srcdir)/version.m4

%.pc: %.pc.in Makefile
	$(SED_PROCESS)

pkginclude_HEADERS = libndctl.h
lib_LTLIBRARIES = lib/libndctl.la

lib_libndctl_la_SOURCES =\
	libndctl.h \
	lib/libndctl-private.h \
	lib/libndctl.c

lib_libndctl_la_LIBADD =\
	$(UDEV_LIBS) \
	$(UUID_LIBS) \
	$(KMOD_LIBS)

if ENABLE_ARS
lib_libndctl_la_SOURCES += lib/libndctl-ars.c
endif

if ENABLE_SMART
lib_libndctl_la_SOURCES += lib/libndctl-smart.c
endif

bin_PROGRAMS = ndctl

ndctl_SOURCES = ndctl.c \
		builtin-create-nfit.c \
		builtin-xaction-namespace.c \
		builtin-xable-region.c \
		builtin-list.c \
		builtin-test.c \
		builtin-help.c \
		builtin-zero-labels.c \
		builtin-read-labels.c \
		util/json.c

if ENABLE_SMART
ndctl_SOURCES += util/json-smart.c
endif

if ENABLE_TEST
ndctl_SOURCES += ../test/libndctl.c \
		 ../test/dpa-alloc.c \
		 ../test/parent-uuid.c \
		 ../test/core.c
endif

if ENABLE_DESTRUCTIVE
ndctl_SOURCES += ../test/blk_namespaces.c \
		 ../test/pmem_namespaces.c \
		 ../test/pcommit.c
ndctl_SOURCES += builtin-bat.c
endif

ndctl_LDADD =\
	lib/libndctl.la \
	../libutil.a \
	$(UUID_LIBS) \
	$(KMOD_LIBS) \
	$(JSON_LIBS)

EXTRA_DIST += lib/libndctl.sym

lib_libndctl_la_LDFLAGS = $(AM_LDFLAGS) \
	-version-info $(LIBNDCTL_CURRENT):$(LIBNDCTL_REVISION):$(LIBNDCTL_AGE) \
	-Wl,--version-script=$(top_srcdir)/ndctl/lib/libndctl.sym
lib_libndctl_la_DEPENDENCIES = lib/libndctl.sym

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = lib/libndctl.pc
EXTRA_DIST += lib/libndctl.pc.in
CLEANFILES += lib/libndctl.pc
