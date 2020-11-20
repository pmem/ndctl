# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

if USE_ASCIIDOCTOR

do_subst = sed -e 's,@Utility@,Daxctl,g' -e's,@utility@,daxctl,g'
CONFFILE = asciidoctor-extensions.rb
asciidoctor-extensions.rb: ../asciidoctor-extensions.rb.in
	$(AM_V_GEN) $(do_subst) < $< > $@

else

do_subst = sed -e 's,UTILITY,daxctl,g'
CONFFILE = asciidoc.conf
asciidoc.conf: ../asciidoc.conf.in
	$(AM_V_GEN) $(do_subst) < $< > $@

endif

man1_MANS = \
	daxctl.1 \
	daxctl-list.1 \
	daxctl-migrate-device-model.1 \
	daxctl-reconfigure-device.1 \
	daxctl-online-memory.1 \
	daxctl-offline-memory.1 \
	daxctl-disable-device.1 \
	daxctl-enable-device.1 \
	daxctl-create-device.1 \
	daxctl-destroy-device.1

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
		-amansource=daxctl -amanmanual="daxctl Manual" \
		-andctl_version=$(VERSION) -o $@+ $< && \
		mv $@+ $@

else

%.xml: %.txt $(XML_DEPS)
	$(AM_V_GEN)$(RM) $@+ $@ && \
		$(ASCIIDOC) -b docbook -d manpage -f asciidoc.conf \
		--unsafe -adaxctl_version=$(VERSION) -o $@+ $< && \
		mv $@+ $@

%.1: %.xml $(XML_DEPS)
	$(AM_V_GEN)$(RM) $@ && \
		$(XMLTO) -o . -m ../manpage-normal.xsl man $<

endif
