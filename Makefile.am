include Makefile.am.in

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
SUBDIRS = . daxctl/lib ndctl/lib ndctl daxctl
if ENABLE_DOCS
SUBDIRS += Documentation/ndctl Documentation/daxctl
endif
SUBDIRS += test

BUILT_SOURCES = version.m4
version.m4: FORCE
	$(AM_V_GEN)$(top_srcdir)/git-version-gen

FORCE:

EXTRA_DIST += ndctl.spec.in \
		sles/header \
		contrib/nvdimm-security.conf

noinst_SCRIPTS = rhel/ndctl.spec sles/ndctl.spec
CLEANFILES += $(noinst_SCRIPTS)

do_rhel_subst = sed -e 's,VERSION,$(VERSION),g' \
            -e 's,DAX_DNAME,daxctl-devel,g' \
            -e 's,DNAME,ndctl-devel,g' \
            -e '/^%defattr.*/d' \
	    -e 's,DAX_LNAME,daxctl-libs,g' \
	    -e 's,LNAME,ndctl-libs,g'

do_sles_subst = sed -e 's,VERSION,$(VERSION),g' \
            -e 's,DAX_DNAME,libdaxctl-devel,g' \
            -e 's,DNAME,libndctl-devel,g' \
            -e 's,%license,%doc,g' \
            -e 's,\(^License:.*GPL\)v2,\1-2.0,g' \
            -e "s,DAX_LNAME,libdaxctl$$(($(LIBDAXCTL_CURRENT) - $(LIBDAXCTL_AGE))),g" \
            -e "s,LNAME,libndctl$$(($(LIBNDCTL_CURRENT) - $(LIBNDCTL_AGE))),g"

rhel/ndctl.spec: ndctl.spec.in Makefile.am version.m4
	$(AM_V_GEN)$(MKDIR_P) rhel; $(do_rhel_subst) < $< > $@

sles/ndctl.spec: sles/header ndctl.spec.in Makefile.am version.m4
	$(AM_V_GEN)$(MKDIR_P) sles; cat sles/header $< | $(do_sles_subst) > $@

if ENABLE_BASH_COMPLETION
bashcompletiondir = $(BASH_COMPLETION_DIR)
dist_bashcompletion_DATA = contrib/ndctl
endif

modprobe_file = contrib/nvdimm-security.conf
modprobedir = $(sysconfdir)/modprobe.d/
modprobe_DATA = $(modprobe_file)

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

noinst_LIBRARIES += libutil.a
libutil_a_SOURCES = \
	util/parse-options.c \
	util/parse-options.h \
	util/usage.c \
	util/size.c \
	util/main.c \
	util/help.c \
	util/strbuf.c \
	util/wrapper.c \
	util/filter.c \
	util/bitmap.c \
	util/abspath.c \
	util/iomem.c \
	util/util.h \
	util/strbuf.h \
	util/size.h \
	util/main.h \
	util/filter.h \
	util/bitmap.h

nobase_include_HEADERS = daxctl/libdaxctl.h
