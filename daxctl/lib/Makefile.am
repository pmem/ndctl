include $(top_srcdir)/Makefile.am.in

%.pc: %.pc.in Makefile
	$(SED_PROCESS)

pkginclude_HEADERS = ../libdaxctl.h
lib_LTLIBRARIES = libdaxctl.la

libdaxctl_la_SOURCES =\
	../libdaxctl.h \
	libdaxctl-private.h \
	../../util/iomem.c \
	../../util/iomem.h \
	../../util/sysfs.c \
	../../util/sysfs.h \
	../../util/log.c \
	../../util/log.h \
	libdaxctl.c

libdaxctl_la_LIBADD =\
	$(UUID_LIBS) \
	$(KMOD_LIBS)

daxctl_modprobe_data_DATA = daxctl.conf

EXTRA_DIST += libdaxctl.sym daxctl.conf

libdaxctl_la_LDFLAGS = $(AM_LDFLAGS) \
	-version-info $(LIBDAXCTL_CURRENT):$(LIBDAXCTL_REVISION):$(LIBDAXCTL_AGE) \
	-Wl,--version-script=$(top_srcdir)/daxctl/lib/libdaxctl.sym
libdaxctl_la_DEPENDENCIES = libdaxctl.sym

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libdaxctl.pc
EXTRA_DIST += libdaxctl.pc.in
CLEANFILES += libdaxctl.pc
