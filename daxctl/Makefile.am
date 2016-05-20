include $(top_srcdir)/Makefile.am.in

%.pc: %.pc.in Makefile
	$(SED_PROCESS)

lib_LTLIBRARIES = lib/libdaxctl.la

lib_libdaxctl_la_SOURCES =\
	libdaxctl.h \
	lib/libdaxctl-private.h \
	../util/log.c \
	../util/log.h \
	lib/libdaxctl.c

EXTRA_DIST += lib/libdaxctl.sym

lib_libdaxctl_la_LDFLAGS = $(AM_LDFLAGS) \
	-version-info $(LIBDAXCTL_CURRENT):$(LIBDAXCTL_REVISION):$(LIBDAXCTL_AGE) \
	-Wl,--version-script=$(top_srcdir)/daxctl/lib/libdaxctl.sym
lib_libdaxctl_la_DEPENDENCIES = lib/libdaxctl.sym

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = lib/libdaxctl.pc
EXTRA_DIST += lib/libdaxctl.pc.in
CLEANFILES += lib/libdaxctl.pc
