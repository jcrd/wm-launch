CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c99 -Wall -Wextra
LDLIBS = $(shell pkgconf --libs xcb)

all: tools/createwindow tools/maprequestwait

tools/createwindow: tools/createwindow.o

tools/maprequestwait: tools/maprequestwait.o

run: all
	./run-tests.sh

clean:
	rm -f tools/createwindow tools/maprequestwait tools/*.o

.PHONY: all run clean
