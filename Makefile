ifeq (,$(wildcard config.mak))
$(error "config.mak is not present, run configure !")
endif
include config.mak

PKGCONFIG_DIR = $(libdir)/pkgconfig
PKGCONFIG_FILE = libvalhalla.pc

TESTVALHALLA = test-valhalla
TESTVALHALLA_SRCS = test-valhalla.c

CFLAGS += -Isrc
LDFLAGS += -Lsrc -lvalhalla -lsqlite3 -lpthread

ifeq ($(BUILD_STATIC),yes)
  LDFLAGS += $(EXTRALIBS)
endif

DISTFILE = libvahalla-$(VERSION).tar.bz2

EXTRADIST = \
	AUTHORS \
	configure \
	COPYING \
	README \
	TODO \

SUBDIRS = \
	DOCS \
	utils \
	src \

all: lib test docs

lib:
	$(MAKE) -C src

test: lib
	$(CC) $(TESTVALHALLA_SRCS) $(OPTFLAGS) $(CFLAGS) $(EXTRACFLAGS) $(LDFLAGS) -o $(TESTVALHALLA)

docs:
	$(MAKE) -C DOCS

docs-clean:
	$(MAKE) -C DOCS clean

clean:
	$(MAKE) -C src clean
	rm -f $(TESTVALHALLA)

distclean: clean docs-clean
	rm -f config.log
	rm -f config.mak
	rm -f $(PKGCONFIG_FILE)

install: install-lib install-pkgconfig install-test install-docs

install-lib: lib
	$(MAKE) -C src install

install-test: test
	$(INSTALL) -d $(bindir)
	$(INSTALL) -c -m 755 $(TESTVALHALLA) $(bindir)

install-docs: docs
	$(MAKE) -C DOCS install

install-pkgconfig: $(PKGCONFIG_FILE)
	$(INSTALL) -d "$(PKGCONFIG_DIR)"
	$(INSTALL) -m 644 $< "$(PKGCONFIG_DIR)"

uninstall: uninstall-lib uninstall-pkgconfig uninstall-test uninstall-docs

uninstall-pkgconfig:
	rm -f $(PKGCONFIG_DIR)/$(PKGCONFIG_FILE)

uninstall-lib:
	$(MAKE) -C src uninstall

uninstall-test:
	rm -f $(bindir)/$(TESTVALHALLA)

uninstall-docs:
	$(MAKE) -C DOCS uninstall

.PHONY: clean distclean
.PHONY: *install*
.PHONY: docs

dist:
	-$(RM) $(DISTFILE)
	dist=$(shell pwd)/libvalhalla-$(VERSION) && \
	for subdir in . $(SUBDIRS); do \
		mkdir -p "$$dist/$$subdir"; \
		$(MAKE) -C $$subdir dist-all DIST="$$dist/$$subdir"; \
	done && \
	tar cjf $(DISTFILE) libvalhalla-$(VERSION)
	-$(RM) -rf libvalhalla-$(VERSION)

dist-all:
	cp $(EXTRADIST) $(TESTVALHALLA_SRCS) Makefile $(DIST)

.PHONY: dist dist-all
