include $(top_srcdir)/Makefile.am.in

bin_PROGRAMS = daxctl

daxctl_SOURCES =\
		daxctl.c \
		list.c \
		../util/json.c

if ENABLE_DAXIO
daxctl_SOURCES += io.c
endif

daxctl_LDADD =\
	lib/libdaxctl.la \
	../ndctl/lib/libndctl.la \
	../libutil.a \
	$(UUID_LIBS) \
	$(JSON_LIBS)

if ENABLE_DAXIO
daxctl_LDADD += $(PMEM_LIBS)
endif
