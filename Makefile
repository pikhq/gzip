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

SRCS=$(wildcard src/*.c)
OBJS=$(SRCS:.c=.o)

PORT=$(wildcard src/port/*.inc)
PORT_SRCS=$(PORT:.inc=.c)
PORT_OBJS=$(PORT:.inc=.o)

LDLIBS+=$(ZLIB_LIBS)
CFLAGS+=$(ZLIB_CFLAGS)
CPPFLAGS+=-Iinclude -D_XOPEN_SOURCE=700 -D_FILE_OFFSET_BITS=64

all: src/gzip

src/gzip: $(OBJS) $(PORT_OBJS)
src/gzip.o: $(wildcard include/*.h)

src/port/%.c: src/port/%.inc src/port/%.fallback tests/%.test.c
	-$(CC) $(LDFLAGS) $(CFLAGS) $(CPPFLAGS) -o /dev/null tests/$*.test.c $(LDLIBS) \
		&& cp src/port/$*.inc src/port/$*.c \
		|| cp src/port/$*.fallback src/port/$*.c

clean:
	-rm -f src/gzip $(OBJS) $(PORT_SRCS) $(PORT_OBJS)
