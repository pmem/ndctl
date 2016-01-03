EXTRA_DIST =
SUBDIRS = . Documentation
CLEANFILES =
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DNDCTL_MAN_PATH=\""$(mandir)"\" \
	-I${top_srcdir}/lib/ndctl \
	-I${top_srcdir}/lib \
	-I${top_srcdir}/ \
	$(KMOD_CFLAGS) \
	$(UDEV_CFLAGS) \
	$(UUID_CFLAGS) \
	$(JSON_CFLAGS)

AM_CFLAGS = ${my_CFLAGS} \
	-fvisibility=hidden \
	-ffunction-sections \
	-fdata-sections

AM_LDFLAGS = \
	-Wl,--gc-sections \
	-Wl,--as-needed

BUILT_SOURCES = $(top_srcdir)/version.m4
$(top_srcdir)/version.m4: FORCE
	$(AM_V_GEN)$(top_srcdir)/git-version-gen

FORCE:

SED_PROCESS = \
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && $(SED) \
	-e 's,@VERSION\@,$(VERSION),g' \
	-e 's,@prefix\@,$(prefix),g' \
	-e 's,@exec_prefix\@,$(exec_prefix),g' \
	-e 's,@libdir\@,$(libdir),g' \
	-e 's,@includedir\@,$(includedir),g' \
	< $< > $@ || rm $@

%.pc: %.pc.in Makefile
	$(SED_PROCESS)

LIBNDCTL_CURRENT=5
LIBNDCTL_REVISION=0
LIBNDCTL_AGE=2

pkginclude_HEADERS = lib/ndctl/libndctl.h
lib_LTLIBRARIES = lib/libndctl.la

lib_libndctl_la_SOURCES =\
	lib/libndctl-private.h \
	lib/libndctl.c
lib_libndctl_la_LIBADD = $(UDEV_LIBS) $(UUID_LIBS) $(KMOD_LIBS)

if ENABLE_ARS
lib_libndctl_la_SOURCES += lib/libndctl-ars.c
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
		util/parse-options.c \
		util/parse-options.h \
		util/usage.c \
		util/json.c \
		util/size.c \
		util/strbuf.c \
		util/wrapper.c \
		util/filter.c

if ENABLE_TEST
ndctl_SOURCES += test/libndctl.c \
		 test/dpa-alloc.c \
		 test/parent-uuid.c \
		 test/core.c
endif

if ENABLE_DESTRUCTIVE
ndctl_SOURCES += test/blk_namespaces.c \
		 test/pmem_namespaces.c \
		 test/pcommit.c
ndctl_SOURCES += builtin-bat.c
endif

ndctl_LDADD = lib/libndctl.la $(UUID_LIBS) $(KMOD_LIBS) $(JSON_LIBS)

noinst_LIBRARIES = libccan.a
libccan_a_SOURCES = \
	ccan/str/str.h \
	ccan/str/str_debug.h \
	ccan/str/str.c \
	ccan/str/debug.c \
	ccan/list/list.h \
	ccan/list/list.c \
	ccan/container_of/container_of.h \
	ccan/check_type/check_type.h \
	ccan/build_assert/build_assert.h \
	ccan/array_size/array_size.h \
	ccan/minmax/minmax.h \
	ccan/short_types/short_types.h \
	ccan/endian/endian.h

EXTRA_DIST += lib/libndctl.sym

lib_libndctl_la_LDFLAGS = $(AM_LDFLAGS) \
	-version-info $(LIBNDCTL_CURRENT):$(LIBNDCTL_REVISION):$(LIBNDCTL_AGE) \
	-Wl,--version-script=$(top_srcdir)/lib/libndctl.sym
lib_libndctl_la_DEPENDENCIES = ${top_srcdir}/lib/libndctl.sym

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = lib/libndctl.pc
EXTRA_DIST += lib/libndctl.pc.in
CLEANFILES += lib/libndctl.pc

TESTS = test/libndctl test/dpa-alloc test/parent-uuid
check_PROGRAMS = test/libndctl test/dpa-alloc test/parent-uuid

if ENABLE_DESTRUCTIVE
TESTS += test/blk-ns test/pmem-ns test/pcommit
check_PROGRAMS += test/blk-ns test/pmem-ns test/pcommit

TESTS += test/dax-dev test/dax.sh test/mmap.sh
check_PROGRAMS += test/dax-dev test/dax-pmd test/mmap
endif

test_libndctl_SOURCES = test/libndctl.c test/core.c
test_libndctl_LDADD = lib/libndctl.la $(UUID_LIBS) $(KMOD_LIBS)

test_pcommit_SOURCES = test/pcommit.c
test_pcommit_LDADD = lib/libndctl.la $(KMOD_LIBS)

test_blk_ns_SOURCES = test/blk_namespaces.c
test_blk_ns_LDADD = lib/libndctl.la $(KMOD_LIBS)

test_pmem_ns_SOURCES = test/pmem_namespaces.c
test_pmem_ns_LDADD = lib/libndctl.la $(KMOD_LIBS)

test_dpa_alloc_SOURCES = test/dpa-alloc.c test/core.c
test_dpa_alloc_LDADD = lib/libndctl.la $(UUID_LIBS) $(KMOD_LIBS)

test_parent_uuid_SOURCES = test/parent-uuid.c test/core.c
test_parent_uuid_LDADD = lib/libndctl.la $(UUID_LIBS) $(KMOD_LIBS)

test_dax_dev_SOURCES = test/dax-dev.c test/core.c
test_dax_dev_LDADD = lib/libndctl.la

test_dax_pmd_SOURCES = test/dax-pmd.c
test_mmap_SOURCES = test/mmap.c
