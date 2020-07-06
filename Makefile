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

MANPAGE = wm-launch.1

all: wm-launch-preload.so wm-launch wm-launchd $(MANPAGE)

compile_commands.json: clean
	bear make

wm-launch-preload.so: wm-launch-preload.o
	$(LINK.c) $(LDLIBS) -o $@ $^

wm-launch: wm-launch.in
	sed -e "s|LIBPREFIX=|LIBPREFIX=$(LIBPREFIX)|" \
		-e "s|VERSION=|VERSION=$(VERSION)|" \
		wm-launch.in > wm-launch
	chmod +x wm-launch

wm-launchd: wm-launchd.go
	go build $<

$(MANPAGE): man/$(MANPAGE).pod
	pod2man -n=wm-launch -c=wm-launch -r=$(VERSION) $< $(MANPAGE)

install:
	mkdir -p $(DESTDIR)$(BINPREFIX)
	cp -p wm-launch $(DESTDIR)$(BINPREFIX)
	cp -p wm-launchd $(DESTDIR)$(BINPREFIX)
	mkdir -p $(DESTDIR)$(LIBPREFIX)/wm-launch
	cp -p wm-launch-preload.so $(DESTDIR)$(LIBPREFIX)/wm-launch
	mkdir -p $(DESTDIR)$(LIBPREFIX)/systemd/user
	cp -p systemd/wm-launchd.service $(DESTDIR)$(LIBPREFIX)/systemd/user
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	cp -p $(MANPAGE) $(DESTDIR)$(MANPREFIX)/man1

uninstall:
	rm -f $(DESTDIR)$(BINPREFIX)/wm-launch
	rm -f $(DESTDIR)$(BINPREFIX)/wm-launchd
	rm -rf $(DESTDIR)$(LIBPREFIX)/wm-launch
	rm -f $(DESTDIR)$(LIBPREFIX)/systemd/user/wm-launchd.service
	rm -f $(DESTDIR)$(MANPREFIX)/man1/$(MANPAGE)

test: all
	$(MAKE) -C test run

test-clean:
	$(MAKE) -C test clean

clean: test-clean
	rm -f wm-launch-preload.so wm-launch-preload.o wm-launch wm-launchd $(MANPAGE)

test-podman: clean
	podman run --rm -v $(shell pwd):/wm-launch:Z -w /wm-launch \
		supplantr/wm-launch make test

.PHONY: all install uninstall test test-clean clean test-podman
