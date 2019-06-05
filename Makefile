VERSIONCMD = git describe --dirty --tags --always 2> /dev/null
VERSION := $(shell $(VERSIONCMD) || cat VERSION)

PREFIX ?= /usr/local
BINPREFIX ?= $(PREFIX)/bin
LIBPREFIX ?= $(PREFIX)/lib
MANPREFIX ?= $(PREFIX)/share/man

X11SONAME ?= libX11.so.6
XCBSONAME ?= libxcb.so.1

CPPFLAGS += -D_POSIX_C_SOURCE=200809L \
			-DX11_SONAME=\"$(X11SONAME)\" -DXCB_SONAME=\"$(XCBSONAME)\"
CFLAGS += -std=c99 -Wall -Wextra -fPIC
LDFLAGS = -shared
LDLIBS = $(shell pkgconf --libs x11-xcb)

all: wm-launch-preload.so wm-launch

wm-launch-preload.so: wm-launch-preload.o
	$(LINK.c) $(LDLIBS) -o $@ $^

wm-launch: wm-launch.in
	sed -e "s/VERSION=/VERSION=$(VERSION)/" wm-launch.in > wm-launch
	chmod +x wm-launch

install:
	mkdir -p $(DESTDIR)$(BINPREFIX)
	cp -p wm-launch $(DESTDIR)$(BINPREFIX)
	mkdir -p $(DESTDIR)$(LIBPREFIX)/wm-launch
	cp -p wm-launch-preload.so $(DESTDIR)$(LIBPREFIX)/wm-launch

uninstall:
	rm -f $(DESTDIR)$(BINPREFIX)/wm-launch
	rm -rf $(DESTDIR)$(LIBPREFIX)/wm-launch

test: all
	$(MAKE) -C test run

test-clean:
	$(MAKE) -C test clean

clean: test-clean
	rm -f wm-launch-preload.so wm-launch-preload.o wm-launch

test-docker: clean
	docker run --rm -v $(shell pwd):/root/wm-launch -w /root/wm-launch \
		supplantr/wm-launch:1.1 make test

.PHONY: all install uninstall test test-clean clean test-docker
