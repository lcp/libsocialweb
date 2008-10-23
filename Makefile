CFLAGS = -g -Wall `pkg-config --cflags gobject-2.0 sqlite3 libsoup-2.4`
LDFLAGS =`pkg-config --libs gobject-2.0 sqlite3 libsoup-2.4`

test: test.c generic.c