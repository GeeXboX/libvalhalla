ifeq (,$(wildcard ../config.mak))
$(error "../config.mak is not present, run configure !")
endif
include ../config.mak

EXTRADIST = \
	Doxyfile \
	valhalla-db.png \
	valhalla-internals.png \

doxygen:
ifeq (,$(wildcard doxygen))
	PROJECT_NUMBER="$(VERSION)" doxygen Doxyfile
endif

dist-all:
	cp $(EXTRADIST) Makefile $(DIST)

.PHONY: dist-all doxygen
