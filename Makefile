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

DOXYGEN =

ifeq ($(DOC),yes)
  DOXYGEN = doxygen
endif

all: lib test $(DOXYGEN)

lib:
	$(MAKE) -C src

test:
	$(CC) $(TESTVALHALLA_SRCS) $(OPTFLAGS) $(CFLAGS) $(EXTRACFLAGS) $(LDFLAGS) -o $(TESTVALHALLA)

doxygen:
ifeq (,$(wildcard DOCS/doxygen))
	PROJECT_NUMBER="$(VERSION)" doxygen DOCS/Doxyfile
endif

clean:
	$(MAKE) -C src clean
	rm -f $(TESTVALHALLA)

distclean: clean
	rm -f config.log
	rm -f config.mak
	rm -f $(PKGCONFIG_FILE)
	rm -rf DOCS/doxygen

install: install-pkgconfig
	$(MAKE) -C src install
	$(INSTALL) -d $(bindir)
	$(INSTALL) -c -m 755 $(TESTVALHALLA) $(bindir)

install-pkgconfig: $(PKGCONFIG_FILE)
	$(INSTALL) -d "$(PKGCONFIG_DIR)"
	$(INSTALL) -m 644 $< "$(PKGCONFIG_DIR)"

uninstall:
	$(MAKE) -C src uninstall
	rm -f $(bindir)/$(TESTVALHALLA)
	rm -f $(PKGCONFIG_DIR)/$(PKGCONFIG_FILE)

.PHONY: clean distclean
.PHONY: install install-pkgconfig uninstall
.PHONY: doxygen
