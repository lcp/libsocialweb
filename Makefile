PKG = sqlite3 libsoup-2.4 libxml-2.0
CFLAGS = -g -Wall `pkg-config --cflags $(PKG)`
LDFLAGS =`pkg-config --libs $(PKG)`

test: test.c generic.c
