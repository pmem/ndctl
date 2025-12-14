# Local development Makefile for ndctl/daxctl

DESTDIR = $(HOME)/.local/ndctl-install
PREFIX = $(HOME)/.local

.PHONY: build install clean

build:
	ninja -C build

install: build
	DESTDIR=$(DESTDIR) ninja -C build install
	cp -a $(DESTDIR)/usr/bin/ndctl $(PREFIX)/bin/ndctl
	cp -a $(DESTDIR)/usr/bin/daxctl $(PREFIX)/bin/daxctl
	cp -a $(DESTDIR)/usr/bin/cxl $(PREFIX)/bin/cxl
	@echo "Installed to $(PREFIX)"
	@$(PREFIX)/bin/daxctl version

clean:
	ninja -C build clean
