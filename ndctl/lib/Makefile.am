include $(top_srcdir)/Makefile.am.in

BUILT_SOURCES = ../libndctl.h
../libndctl.h: ../libndctl.h.in
	touch $(top_srcdir)/version.m4

%.pc: %.pc.in Makefile
	$(SED_PROCESS)

pkginclude_HEADERS = ../libndctl.h
lib_LTLIBRARIES = libndctl.la

libndctl_la_SOURCES =\
	libndctl.h \
	private.h \
	../../util/log.c \
	../../util/log.h \
	../../util/sysfs.c \
	../../util/sysfs.h \
	dimm.c \
	inject.c \
	libndctl.c

libndctl_la_LIBADD =\
	../../daxctl/lib/libdaxctl.la \
	$(UDEV_LIBS) \
	$(UUID_LIBS) \
	$(KMOD_LIBS)

if ENABLE_ARS
libndctl_la_SOURCES += ars.c
endif

if ENABLE_SMART
libndctl_la_SOURCES += smart.c
libndctl_la_SOURCES += hpe1.c
libndctl_la_SOURCES += msft.c
endif

libndctl_la_SOURCES += nfit.c

EXTRA_DIST += libndctl.sym

libndctl_la_LDFLAGS = $(AM_LDFLAGS) \
	-version-info $(LIBNDCTL_CURRENT):$(LIBNDCTL_REVISION):$(LIBNDCTL_AGE) \
	-Wl,--version-script=$(top_srcdir)/ndctl/lib/libndctl.sym
libndctl_la_DEPENDENCIES = libndctl.sym

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libndctl.pc
EXTRA_DIST += libndctl.pc.in
CLEANFILES += libndctl.pc
