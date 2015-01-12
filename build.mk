CC?=c99
CFLAGS?=-O2
ifeq ($(findstring gcc,$(CC)), gcc)
	override CC+=-std=c99
endif
ifeq (cc,$(CC))
	override CC=c99
endif
ifeq (mingw,$(findstring mingw,$(CC)))
	SOSUFFIX=dll
endif
ifeq (darwin,$(findstring darwin,$(CC)))
	SOSUFFIX=dylib
endif
ifeq (,$(SOSUFFIX))
ifeq ($(shell uname -s),)
	SOSUFFIX=dll
endif
ifeq ($(shell uname -s),Darwin)
	SOSUFFIX=dylib
endif
ifeq (,$(SOSUFFIX))
	SOSUFFIX=so
endif
endif

%.a:
	ar rcs $@ $^
	ranlib $@
%.so:
	$(CC) $(LDFLAGS) -shared $^ $(LOADLIBES) $(LDLIBS) -o $@
%.dylib:
	$(CC) $(LDFLAGS) -dynamiclib $^ $(LOADLIBES) $(LDLIBS) -o $@
%.dll:
	$(CC) $(LDFLAGS) -Wl,--export-all $^ $(LOADLIBES) $(LDLIBS) -o $@

%.lo: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $< -o $@

