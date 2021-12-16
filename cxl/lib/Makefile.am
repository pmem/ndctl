include $(top_srcdir)/Makefile.am.in

%.pc: %.pc.in Makefile
	$(SED_PROCESS)

pkginclude_HEADERS = ../libcxl.h ../cxl_mem.h
lib_LTLIBRARIES = libcxl.la

libcxl_la_SOURCES =\
	../libcxl.h \
	private.h \
	../../util/sysfs.c \
	../../util/sysfs.h \
	../../util/log.c \
	../../util/log.h \
	libcxl.c

libcxl_la_LIBADD =\
	$(UUID_LIBS) \
	$(KMOD_LIBS)

EXTRA_DIST += libcxl.sym

libcxl_la_LDFLAGS = $(AM_LDFLAGS) \
	-version-info $(LIBCXL_CURRENT):$(LIBCXL_REVISION):$(LIBCXL_AGE) \
	-Wl,--version-script=$(top_srcdir)/cxl/lib/libcxl.sym
libcxl_la_DEPENDENCIES = libcxl.sym

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcxl.pc
EXTRA_DIST += libcxl.pc.in
CLEANFILES += libcxl.pc
