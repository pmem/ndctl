man1_MANS = \
	ndctl.1 \
	ndctl-zero-labels.1 \
	ndctl-enable-region.1 \
	ndctl-disable-region.1 \
	ndctl-enable-namespace.1 \
	ndctl-disable-namespace.1 \
	ndctl-create-namespace.1

XML_DEPS = \
	$(top_srcdir)/version.m4 \
	Makefile \
	region-description.txt \
	xable-region-options.txt \
	xable-namespace-options.txt

RM ?= rm -f

%.xml: %.txt $(XML_DEPS)
	$(AM_V_GEN)$(RM) $@+ $@ && \
		$(ASCIIDOC) -b docbook -d manpage -f asciidoc.conf \
		--unsafe -andctl_version=$(VERSION) -o $@+ $< && \
		mv $@+ $@

%.1: %.xml
	$(AM_V_GEN)$(RM) $@ && \
		$(XMLTO) -o . -m manpage-normal.xsl man $<
