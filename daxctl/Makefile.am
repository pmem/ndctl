include $(top_srcdir)/Makefile.am.in

bin_PROGRAMS = daxctl

daxctl_SOURCES =\
		daxctl.c \
		list.c \
		../util/json.c

daxctl_LDADD =\
	lib/libdaxctl.la \
	../libutil.a \
	$(JSON_LIBS)
