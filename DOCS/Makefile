ifeq (,$(wildcard ../config.mak))
$(error "../config.mak is not present, run configure !")
endif
include ../config.mak

EXTRADIST = \
	Doxyfile \
	valhalla-db.png \
	valhalla-internals.png \

all: doxygen

doxygen:
ifeq (,$(wildcard doxygen))
	PROJECT_NUMBER="$(VERSION)" doxygen Doxyfile
endif

clean:
	rm -rf doxygen

dist-all:
	cp $(EXTRADIST) Makefile $(DIST)

.PHONY: clean dist-all doxygen
