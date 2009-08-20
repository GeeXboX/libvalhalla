ifeq (,$(wildcard ../config.mak))
$(error "../config.mak is not present, run configure !")
endif
include ../config.mak

ifeq ($(BUILD_STATIC),yes)
  BUILD_RULES += lib_static
endif
ifeq ($(BUILD_SHARED),yes)
  BUILD_RULES += lib_shared
endif

LIBNAME = libvalhalla
STATIC_LIBNAME = ${LIBNAME}.a
SHARED_LIBNAME = ${LIBNAME}.so
SHARED_LIBNAME_VERSION = ${SHARED_LIBNAME}.${VERSION}
SHARED_LIBNAME_MAJOR = $(SHARED_LIBNAME).$(shell echo $(VERSION) | cut -f1 -d.)

SRCS =  database.c \
	dbmanager.c \
	dispatcher.c \
	fifo_queue.c \
	lavf_utils.c \
	logs.c \
	metadata.c \
	parser.c \
	scanner.c \
	thread_utils.c \
	timer_thread.c \
	utils.c \
	valhalla.c \

ifeq ($(GRABBER),yes)
  SRCS += downloader.c \
          grabber.c \
          list.c \
          url_utils.c \

ifeq ($(XML),yes)
  SRCS += xml_utils.c
endif
ifeq ($(MD5),yes)
  SRCS += md5.c
endif
ifeq ($(GRABBER_DUMMY),yes)
  SRCS += grabber_dummy.c
endif
ifeq ($(GRABBER_ALLOCINE),yes)
  SRCS += grabber_allocine.c
endif
ifeq ($(GRABBER_AMAZON),yes)
  SRCS += grabber_amazon.c
endif
ifeq ($(GRABBER_EXIF),yes)
  SRCS += grabber_exif.c
endif
ifeq ($(GRABBER_LYRICWIKI),yes)
  SRCS += grabber_lyricwiki.c
endif
ifeq ($(GRABBER_TMDB),yes)
  SRCS += grabber_tmdb.c
endif
ifeq ($(GRABBER_TVRAGE),yes)
  SRCS += grabber_tvrage.c
endif
endif

OBJS = $(SRCS:.c=.o)

.SUFFIXES: .c .o

all: depend $(BUILD_RULES)

.c.o:
	$(CC) -c $(CFLAGS) $(EXTRACFLAGS) $(OPTFLAGS) -o $@ $<

lib_static: $(OBJS)
	$(AR) r $(STATIC_LIBNAME) $(OBJS)
	$(RANLIB) $(STATIC_LIBNAME)

lib_shared: $(OBJS)
	$(CC) -shared -Wl,-soname,$(SHARED_LIBNAME_MAJOR) \
	  $(OBJS) $(LDFLAGS) $(EXTRALIBS) -o $(SHARED_LIBNAME_VERSION)
	$(LN) -sf $(SHARED_LIBNAME_VERSION) $(SHARED_LIBNAME_MAJOR)
	$(LN) -sf $(SHARED_LIBNAME_MAJOR) $(SHARED_LIBNAME)

clean:
	rm -f *.o *.a *.so*
	rm -f .depend

install: $(BUILD_RULES)
	$(INSTALL) -d $(libdir)
	[ $(BUILD_STATIC) = yes ] && $(INSTALL) -c $(STATIC_LIBNAME) $(libdir); \
	if [ $(BUILD_SHARED) = yes ]; then \
	  $(INSTALL) -c $(SHARED_LIBNAME_VERSION) $(libdir); \
	  $(LN) -sf $(SHARED_LIBNAME_VERSION) $(libdir)/$(SHARED_LIBNAME_MAJOR); \
	  $(LN) -sf $(SHARED_LIBNAME_MAJOR) $(libdir)/$(SHARED_LIBNAME); \
	fi
	$(INSTALL) -d $(includedir)
	$(INSTALL) -c -m 644 valhalla.h $(includedir)

uninstall:
	rm -f $(libdir)/$(STATIC_LIBNAME)
	rm -f $(libdir)/$(SHARED_LIBNAME)*
	rm -f $(includedir)/valhalla.h

depend:
	$(CC) -MM $(CFLAGS) $(EXTRACFLAGS) $(SRCS) 1>.depend

.PHONY: clean depend
.PHONY: uninstall

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
