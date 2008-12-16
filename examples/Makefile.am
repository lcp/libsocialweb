noinst_PROGRAMS = test-client

test_client_CFLAGS = $(DBUS_GLIB_CFLAGS) -I$(top_srcdir)
test_client_LDFLAGS = $(DBUS_GLIB_LIBS) \
		      $(top_builddir)/mojito-client/libmojito-client.la
test_client_SOURCES = test-client.c
EXTRA_DIST = monitor.py
