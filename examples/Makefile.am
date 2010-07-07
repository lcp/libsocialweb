noinst_PROGRAMS = monitor-caps test-client-query

test_client_query_CFLAGS = $(DBUS_GLIB_CFLAGS) -I$(top_srcdir)
test_client_query_LDFLAGS = $(DBUS_GLIB_LIBS) \
		      $(top_builddir)/libsocialweb-client/libsocialweb-client.la
test_client_query_SOURCES = test-client-query.c

monitor_caps_CFLAGS = $(DBUS_GLIB_CFLAGS) -I$(top_srcdir)
monitor_caps_LDFLAGS = $(DBUS_GLIB_LIBS) \
		      $(top_builddir)/libsocialweb-client/libsocialweb-client.la
monitor_caps_SOURCES = monitor-caps.c

EXTRA_DIST = monitor.py
