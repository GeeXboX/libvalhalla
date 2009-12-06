ifeq (,$(wildcard config.mak))
$(error "config.mak is not present, run configure !")
endif
include config.mak

PKGCONFIG_DIR = $(libdir)/pkgconfig
PKGCONFIG_FILE = libvalhalla.pc

VHTEST = libvalhalla-test
VHTEST_SRCS = libvalhalla-test.c

CFLAGS += -Isrc
LDFLAGS += -Lsrc -lvalhalla -lsqlite3 -lpthread

ifeq ($(BUILD_STATIC),yes)
  LDFLAGS += $(EXTRALIBS)
endif

DISTFILE = libvalhalla-$(VERSION).tar.bz2

EXTRADIST = \
	AUTHORS \
	ChangeLog \
	configure \
	COPYING \
	README \
	TODO \

SUBDIRS = \
	DOCS \
	src \
	utils \

all: lib test docs

lib:
	$(MAKE) -C src

test: lib
	$(CC) $(VHTEST_SRCS) $(OPTFLAGS) $(CFLAGS) $(EXTRACFLAGS) $(LDFLAGS) -o $(VHTEST)

docs:
	$(MAKE) -C DOCS

docs-clean:
	$(MAKE) -C DOCS clean

clean:
	$(MAKE) -C src clean
	rm -f $(VHTEST)

distclean: clean docs-clean
	rm -f config.log
	rm -f config.mak
	rm -f $(DISTFILE)
	rm -f $(PKGCONFIG_FILE)

install: install-lib install-pkgconfig install-test install-docs

install-lib: lib
	$(MAKE) -C src install

install-pkgconfig: $(PKGCONFIG_FILE)
	$(INSTALL) -d "$(PKGCONFIG_DIR)"
	$(INSTALL) -m 644 $< "$(PKGCONFIG_DIR)"

install-test: test
	$(INSTALL) -d $(bindir)
	$(INSTALL) -c -m 755 $(VHTEST) $(bindir)

install-docs: docs
	$(MAKE) -C DOCS install

uninstall: uninstall-lib uninstall-pkgconfig uninstall-test uninstall-docs

uninstall-lib:
	$(MAKE) -C src uninstall

uninstall-pkgconfig:
	rm -f $(PKGCONFIG_DIR)/$(PKGCONFIG_FILE)

uninstall-test:
	rm -f $(bindir)/$(VHTEST)

uninstall-docs:
	$(MAKE) -C DOCS uninstall

.PHONY: *clean *install* docs

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
	cp $(EXTRADIST) $(VHTEST_SRCS) Makefile $(DIST)

.PHONY: dist dist-all
