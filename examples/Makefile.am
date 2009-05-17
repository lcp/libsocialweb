noinst_PROGRAMS = test-client monitor-caps

test_client_CFLAGS = $(DBUS_GLIB_CFLAGS) -I$(top_srcdir)
test_client_LDFLAGS = $(DBUS_GLIB_LIBS) \
		      $(top_builddir)/mojito-client/libmojito-client.la
test_client_SOURCES = test-client.c

monitor_caps_CFLAGS = $(DBUS_GLIB_CFLAGS) -I$(top_srcdir)
monitor_caps_LDFLAGS = $(DBUS_GLIB_LIBS) \
		      $(top_builddir)/mojito-client/libmojito-client.la
monitor_caps_SOURCES = monitor-caps.c

EXTRA_DIST = monitor.py
