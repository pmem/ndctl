# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020-2021 Intel Corporation. All rights reserved.

if USE_ASCIIDOCTOR

do_subst = sed -e 's,@Utility@,Libcxl,g' -e's,@utility@,libcxl,g'
CONFFILE = asciidoctor-extensions.rb
asciidoctor-extensions.rb: ../../asciidoctor-extensions.rb.in
	$(AM_V_GEN) $(do_subst) < $< > $@

else

do_subst = sed -e 's,UTILITY,libcxl,g'
CONFFILE = asciidoc.conf
asciidoc.conf: ../../asciidoc.conf.in
	$(AM_V_GEN) $(do_subst) < $< > $@

endif

man3_MANS = \
	libcxl.3 \
	cxl_new.3

EXTRA_DIST = $(man3_MANS)

CLEANFILES = $(man3_MANS)

XML_DEPS = \
	../../../version.m4 \
	../../copyright.txt \
	Makefile \
	$(CONFFILE)

RM ?= rm -f

if USE_ASCIIDOCTOR

%.3: %.txt $(XML_DEPS)
	$(AM_V_GEN)$(RM) $@+ $@ && \
		$(ASCIIDOC) -b manpage -d manpage -acompat-mode \
		-I. -rasciidoctor-extensions \
		-amansource=libcxl -amanmanual="libcxl Manual" \
		-andctl_version=$(VERSION) -o $@+ $< && \
		mv $@+ $@

else

%.xml: %.txt $(XML_DEPS)
	$(AM_V_GEN)$(RM) $@+ $@ && \
		$(ASCIIDOC) -b docbook -d manpage -f asciidoc.conf \
		--unsafe -alibcxl_version=$(VERSION) -o $@+ $< && \
		mv $@+ $@

%.3: %.xml $(XML_DEPS)
	$(AM_V_GEN)$(RM) $@ && \
		$(XMLTO) -o . -m ../../manpage-normal.xsl man $<

endif
