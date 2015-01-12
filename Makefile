include build.mk

PKG_CONFIG?=pkg-config

ifeq ($(ZLIB_LIBS),)
ZLIB_LIBS:=$(shell $(CROSS_COMPILE)$(PKG_CONFIG) zlib --libs)
ZLIB_CFLAGS:=$(shell $(CROSS_COMPILE)$(PKG_CONFIG) zlib --cflags)
ifeq ($(ZLIB_LIBS),)
# If pkg-config didn't tell us anything, try -lz anyways
ZLIB_LIBS:=-lz
ZLIB_CFLAGS:=
endif
endif

LDLIBS+=$(ZLIB_LIBS)
CFLAGS+=$(ZLIB_CFLAGS)

all: src/gzip

src/gzip: src/gzip.o

clean:
	-rm -f src/gzip src/gzip.o
