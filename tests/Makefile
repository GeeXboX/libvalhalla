ifeq (,$(wildcard ../config.mak))
$(error "../config.mak is not present, run configure !")
endif
include ../config.mak

VH_TEST = vh_test

APP_CPPFLAGS = -I../src $(CFG_CPPFLAGS) $(CPPFLAGS)
APP_LDFLAGS = -L../src -lcheck $(CFG_LDFLAGS) $(LDFLAGS)

APP_CPPFLAGS += -DOSDEP_STRNDUP -DOSDEP_STRCASESTR -DOSDEP_STRTOK_R

SRCS =  vh_suite.c \
	vh_test_osdep.c \

EXTRA_SRCS = \
	osdep.c \

EXTRADIST = \
	vh_test.h \

OBJS = $(SRCS:.c=.o) $(EXTRA_SRCS:.c=.o)

.SUFFIXES: .c .o

all: extra_srcs depend $(VH_TEST)

.c.o:
	$(CC) -c $(OPTFLAGS) $(CFLAGS) $(APP_CPPFLAGS) -o $@ $<

$(VH_TEST): $(OBJS)
	$(CC) $(OBJS) $(APP_LDFLAGS) -o $(VH_TEST)

extra_srcs:
	for l in $(EXTRA_SRCS); do \
	  ln -sf ../src/$$l ./; \
	done

clean:
	rm -f $(EXTRA_SRCS)
	rm -f *.o
	rm -f $(VH_TEST)
	rm -f .depend

depend:
	$(CC) -MM $(CFLAGS) $(CFG_CPPFLAGS) $(APP_CPPFLAGS) $(SRCS) $(EXTRA_SRCS) 1>.depend

.PHONY: clean depend extra_srcs $(EXTRA_SRCS)

dist-all:
	cp $(EXTRADIST) $(SRCS) Makefile $(DIST)

.PHONY: dist-all

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
