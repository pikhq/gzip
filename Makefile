CC=c99
CFLAGS=-O2
LDFLAGS=-s

ifeq ($(ZLIB_LIBS),)
ZLIB_LIBS:=$(shell $(CROSS_COMPILE)pkg-config zlib --libs)
ZLIB_CFLAGS:=$(shell $(CROSS_COMPILE)pkg-config zlib --cflags)
ifeq ($(ZLIB_LIBS),)
# If pkg-config didn't tell us anything, try -lz anyways
ZLIB_LIBS:=-lz
ZLIB_CFLAGS:=
endif
endif

LDLIBS+=$(ZLIB_LIBS)
CFLAGS+=$(ZLIB_CFLAGS)

all: gzip

gzip: gzip.o

clean:
	-rm -f gzip gzip.o
