EXTRA_DIST =
CLEANFILES =
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	-I${top_srcdir}/lib/ndctl \
	-I${top_srcdir}/lib \
	-I${top_srcdir}/

AM_CFLAGS = ${my_CFLAGS} \
	-fvisibility=hidden \
	-ffunction-sections \
	-fdata-sections

AM_LDFLAGS = \
	-Wl,--gc-sections \
	-Wl,--as-needed

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

LIBNDCTL_CURRENT=2
LIBNDCTL_REVISION=0
LIBNDCTL_AGE=2

pkginclude_HEADERS = lib/ndctl/libndctl.h
lib_LTLIBRARIES = lib/libndctl.la

lib_libndctl_la_SOURCES =\
	lib/libndctl-private.h \
	lib/libndctl.c
lib_libndctl_la_LIBADD = $(LIBUDEV) $(LIBUUID) $(LIBKMOD)

bin_PROGRAMS = ndctl

ndctl_SOURCES = ndctl.c \
		builtin-create-nfit.c \
		builtin-xable-namespace.c \
		builtin-xable-region.c \
		util/parse-options.c \
		util/parse-options.h \
		util/usage.c \
		util/strbuf.c \
		util/strbuf.h \
		util/wrapper.c
ndctl_LDADD = lib/libndctl.la

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
	ccan/minmax/minmax.h

EXTRA_DIST += lib/libndctl.sym

lib_libndctl_la_LDFLAGS = $(AM_LDFLAGS) \
	-version-info $(LIBNDCTL_CURRENT):$(LIBNDCTL_REVISION):$(LIBNDCTL_AGE) \
	-Wl,--version-script=$(top_srcdir)/lib/libndctl.sym
lib_libndctl_la_DEPENDENCIES = ${top_srcdir}/lib/libndctl.sym

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = lib/libndctl.pc
EXTRA_DIST += lib/libndctl.pc.in
CLEANFILES += lib/libndctl.pc

TESTS = lib/test-libndctl

check_PROGRAMS = lib/test-libndctl
lib_test_libndctl_SOURCES = lib/test-libndctl.c
lib_test_libndctl_LDADD = lib/libndctl.la \
	$(LIBUUID) $(LIBKMOD)
