CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c99 -Wall -Wextra
LDLIBS = $(shell pkgconf --libs xcb)

all: maprequestwait

maprequestwait: maprequestwait.o

run: all
	xvfb-run ./test.sh

clean:
	rm -f maprequestwait.o maprequestwait *.log

.PHONY: all run clean
