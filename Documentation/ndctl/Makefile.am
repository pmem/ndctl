# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

if USE_ASCIIDOCTOR

do_subst = sed -e 's,@Utility@,Ndctl,g' -e's,@utility@,ndctl,g'
CONFFILE = asciidoctor-extensions.rb
asciidoctor-extensions.rb: ../asciidoctor-extensions.rb.in
	$(AM_V_GEN) $(do_subst) < $< > $@

else

do_subst = sed -e 's,UTILITY,ndctl,g'
CONFFILE = asciidoc.conf
asciidoc.conf: ../asciidoc.conf.in
	$(AM_V_GEN) $(do_subst) < $< > $@

endif

man1_MANS = \
	ndctl.1 \
	ndctl-wait-scrub.1 \
	ndctl-start-scrub.1 \
	ndctl-zero-labels.1 \
	ndctl-read-labels.1 \
	ndctl-write-labels.1 \
	ndctl-init-labels.1 \
	ndctl-check-labels.1 \
	ndctl-enable-region.1 \
	ndctl-disable-region.1 \
	ndctl-enable-dimm.1 \
	ndctl-disable-dimm.1 \
	ndctl-enable-namespace.1 \
	ndctl-disable-namespace.1 \
	ndctl-create-namespace.1 \
	ndctl-destroy-namespace.1 \
	ndctl-check-namespace.1 \
	ndctl-clear-errors.1 \
	ndctl-inject-error.1 \
	ndctl-inject-smart.1 \
	ndctl-update-firmware.1 \
	ndctl-list.1 \
	ndctl-monitor.1 \
	ndctl-setup-passphrase.1 \
	ndctl-update-passphrase.1 \
	ndctl-remove-passphrase.1 \
	ndctl-freeze-security.1 \
	ndctl-sanitize-dimm.1 \
	ndctl-load-keys.1 \
	ndctl-wait-overwrite.1 \
	ndctl-read-infoblock.1 \
	ndctl-write-infoblock.1 \
	ndctl-activate-firmware.1

EXTRA_DIST = $(man1_MANS)

CLEANFILES = $(man1_MANS)

.ONESHELL:
attrs.adoc: $(srcdir)/Makefile.am
	$(AM_V_GEN) cat <<- EOF >$@
		:ndctl_monitorconfdir: $(ndctl_monitorconfdir)
		:ndctl_monitorconf: $(ndctl_monitorconf)
		:ndctl_keysdir: $(ndctl_keysdir)
		EOF

XML_DEPS = \
	../../version.m4 \
	Makefile \
	$(CONFFILE) \
	../copyright.txt \
	region-description.txt \
	xable-region-options.txt \
	dimm-description.txt \
	xable-dimm-options.txt \
	xable-namespace-options.txt \
	ars-description.txt \
	labels-description.txt \
	labels-options.txt \
	attrs.adoc

RM ?= rm -f

if USE_ASCIIDOCTOR

%.1: %.txt $(XML_DEPS)
	$(AM_V_GEN)$(RM) $@+ $@ && \
		$(ASCIIDOC) -b manpage -d manpage -acompat-mode \
		-I. -rasciidoctor-extensions \
		-amansource=ndctl -amanmanual="ndctl Manual" \
		-andctl_version=$(VERSION) -o $@+ $< && \
		mv $@+ $@

else

%.xml: %.txt $(XML_DEPS)
	$(AM_V_GEN)$(RM) $@+ $@ && \
		$(ASCIIDOC) -b docbook -d manpage -f asciidoc.conf \
		--unsafe -andctl_version=$(VERSION) -o $@+ $< && \
		mv $@+ $@

%.1: %.xml $(XML_DEPS)
	$(AM_V_GEN)$(RM) $@ && \
		$(XMLTO) -o . -m ../manpage-normal.xsl man $<

endif
