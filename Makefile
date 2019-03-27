PREFIX ?= /usr/local
BINPREFIX ?= $(PREFIX)/bin
LIBPREFIX ?= $(PREFIX)/lib
MANPREFIX ?= $(PREFIX)/share/man

CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c99 -Wall -Wextra -fPIC
LDFLAGS = -shared
LDLIBS = $(shell pkgconf --libs x11-xcb)

all: wm-launch-preload.so

debug: CFLAGS += -DDEBUG -O0 -g
debug: wm-launch-preload.so

wm-launch-preload.so: wm-launch-preload.o
	$(LINK.c) $(LDLIBS) -o $@ $^

install:
	mkdir -p $(DESTDIR)$(BINPREFIX)
	cp -p wm-launch $(DESTDIR)$(BINPREFIX)
	mkdir -p $(DESTDIR)$(LIBPREFIX)/wm-launch
	cp -p wm-launch-preload.so $(DESTDIR)$(LIBPREFIX)/wm-launch

uninstall:
	rm -f $(DESTDIR)$(BINPREFIX)/wm-launch
	rm -rf $(DESTDIR)$(LIBPREFIX)/wm-launch

clean:
	rm -f wm-launch-preload.so wm-launch-preload.o

.PHONY: all debug install uninstall clean
