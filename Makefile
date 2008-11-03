PKG = sqlite3 libsoup-2.4 libxml-2.0
CFLAGS = -g -Wall `pkg-config --cflags $(PKG)`
LDFLAGS =`pkg-config --libs $(PKG)`

ifeq ($(NOCACHE), 1)
CFLAGS += -DNO_CACHE
endif

test: test.c generic.c

clean:
	rm -f test

.PHONY: clean
