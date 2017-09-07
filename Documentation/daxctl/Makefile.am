# Copyright(c) 2015-2017 Intel Corporation.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.

do_subst = sed -e 's,UTILITY,daxctl,g'

asciidoc.conf: ../asciidoc.conf.in
	$(AM_V_GEN) $(do_subst) < $< > $@

man1_MANS = \
	daxctl.1 \
	daxctl-list.1 \
	daxctl-io.1

CLEANFILES = $(man1_MANS)

XML_DEPS = \
	../../version.m4 \
	Makefile \
	asciidoc.conf

RM ?= rm -f

%.xml: %.txt $(XML_DEPS)
	$(AM_V_GEN)$(RM) $@+ $@ && \
		$(ASCIIDOC) -b docbook -d manpage -f asciidoc.conf \
		--unsafe -adaxctl_version=$(VERSION) -o $@+ $< && \
		mv $@+ $@

%.1: %.xml
	$(AM_V_GEN)$(RM) $@ && \
		$(XMLTO) -o . -m ../manpage-normal.xsl man $<
