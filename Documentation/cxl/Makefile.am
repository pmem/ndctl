# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020-2021 Intel Corporation. All rights reserved.

if USE_ASCIIDOCTOR

do_subst = sed -e 's,@Utility@,Cxl,g' -e's,@utility@,cxl,g'
CONFFILE = asciidoctor-extensions.rb
asciidoctor-extensions.rb: ../asciidoctor-extensions.rb.in
	$(AM_V_GEN) $(do_subst) < $< > $@

else

do_subst = sed -e 's,UTILITY,cxl,g'
CONFFILE = asciidoc.conf
asciidoc.conf: ../asciidoc.conf.in
	$(AM_V_GEN) $(do_subst) < $< > $@

endif

man1_MANS = \
	cxl.1 \
	cxl-list.1 \
	cxl-read-labels.1 \
	cxl-write-labels.1 \
	cxl-zero-labels.1

EXTRA_DIST = $(man1_MANS)

CLEANFILES = $(man1_MANS)

XML_DEPS = \
	../../version.m4 \
	../copyright.txt \
	Makefile \
	$(CONFFILE)

RM ?= rm -f

if USE_ASCIIDOCTOR

%.1: %.txt $(XML_DEPS)
	$(AM_V_GEN)$(RM) $@+ $@ && \
		$(ASCIIDOC) -b manpage -d manpage -acompat-mode \
		-I. -rasciidoctor-extensions \
		-amansource=cxl -amanmanual="cxl Manual" \
		-andctl_version=$(VERSION) -o $@+ $< && \
		mv $@+ $@

else

%.xml: %.txt $(XML_DEPS)
	$(AM_V_GEN)$(RM) $@+ $@ && \
		$(ASCIIDOC) -b docbook -d manpage -f asciidoc.conf \
		--unsafe -acxl_version=$(VERSION) -o $@+ $< && \
		mv $@+ $@

%.1: %.xml $(XML_DEPS)
	$(AM_V_GEN)$(RM) $@ && \
		$(XMLTO) -o . -m ../manpage-normal.xsl man $<

endif
