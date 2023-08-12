ifeq (,$(wildcard ../config.mak))
$(error "../config.mak is not present, run configure !")
endif
include ../config.mak

EXTRADIST = \
	curl.png \
	Doxyfile \
	ffmpeg.png \
	priorities.txt \
	sqlite3.png \
	states_ffmpeg_01.dia \
	states_general.dia \
	states_grabbers_01.dia \
	states_grabbers_lock.dia \
	states_ondemand_01.dia \
	valhalla.eps \
	valhalla.lyx \
	valhalla.svg \
	valhalla-db.dia \
	valhalla-db.png \
	valhalla-internals.dia \
	valhalla-internals.png \

all: doxygen lyx

doxygen:
ifeq ($(DOC),yes)
ifeq ($(DOC_DOXYGEN),yes)
ifeq (,$(wildcard doxygen))
	PROJECT_NUMBER="$(VERSION)" doxygen Doxyfile
endif
endif
endif

lyx:
ifeq ($(DOC),yes)
ifeq ($(DOC_LYX),yes)
ifeq (,$(wildcard valhalla-$(VERSION).pdf))
	sed 's,filename valhalla-internals.dia,filename valhalla-internals.tmp.dia,' valhalla.lyx > valhalla.tmp.lyx
	sed -e 's,\./curl.png,$(shell pwd)/curl.png,' \
	    -e 's,\./ffmpeg.png,$(shell pwd)/ffmpeg.png,' \
	    -e 's,\./sqlite3.png,$(shell pwd)/sqlite3.png,' \
            valhalla-internals.dia > valhalla-internals.tmp.dia
	lyx valhalla.tmp.lyx -e pdf2
	mv valhalla.tmp.pdf valhalla-$(VERSION).pdf
	rm -f *.tmp.*
endif
endif
endif

clean:
	rm -rf doxygen
	rm -f valhalla-$(VERSION).pdf

install:
ifeq ($(DOC),yes)
	if [ -d doxygen/html ]; then \
		$(INSTALL) -d $(docdir)/libvalhalla; \
		$(INSTALL) -c -m 755 doxygen/html/* $(docdir)/libvalhalla; \
	fi
endif

uninstall:
	rm -rf $(docdir)/libvalhalla

dist-all:
	cp $(EXTRADIST) Makefile $(DIST)

.PHONY: clean dist-all doxygen *install lyx
