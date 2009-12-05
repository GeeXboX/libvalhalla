ifeq (,$(wildcard ../config.mak))
$(error "../config.mak is not present, run configure !")
endif
include ../config.mak

EXTRADIST = \
	searchMovieAllocine.php \
	searchMovieIMDB.php \

dist-all:
	cp $(EXTRADIST) Makefile $(DIST)

.PHONY: dist-all
