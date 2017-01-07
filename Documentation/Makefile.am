man1_MANS = \
	ndctl.1 \
	ndctl-zero-labels.1 \
	ndctl-read-labels.1 \
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
	ndctl-list.1 \
	daxctl-list.1

CLEANFILES = $(man1_MANS)

XML_DEPS = \
	..//version.m4 \
	Makefile \
	region-description.txt \
	xable-region-options.txt \
	dimm-description.txt \
	xable-dimm-options.txt \
	xable-namespace-options.txt \
	labels-description.txt \
	labels-options.txt

RM ?= rm -f

%.xml: %.txt $(XML_DEPS)
	$(AM_V_GEN)$(RM) $@+ $@ && \
		$(ASCIIDOC) -b docbook -d manpage -f asciidoc.conf \
		--unsafe -andctl_version=$(VERSION) -o $@+ $< && \
		mv $@+ $@

%.1: %.xml
	$(AM_V_GEN)$(RM) $@ && \
		$(XMLTO) -o . -m manpage-normal.xsl man $<
