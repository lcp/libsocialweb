noinst_PROGRAMS = test-online test-client-online test-download test-download-async

test_online_SOURCES = test-online.c
test_online_CFLAGS = -I$(top_srcdir) $(GOBJECT_CFLAGS)
test_online_LDADD = $(GOBJECT_LIBS) ../libsocialweb/libsocialweb.la

test_client_online_SOURCES = test-client-online.c
test_client_online_CFLAGS = -I$(top_srcdir) $(GOBJECT_CFLAGS)
test_client_online_LDADD = $(GOBJECT_LIBS) ../libsocialweb-client/libsocialweb-client.la

test_download_SOURCES = test-download.c
test_download_CFLAGS = -I$(top_srcdir) $(GOBJECT_CFLAGS) $(SOUP_CFLAGS) $(DBUS_GLIB_CFLAGS)
test_download_LDADD = $(GOBJECT_LIBS) $(SOUP_LIBS) ../libsocialweb/libsocialweb.la

test_download_async_SOURCES = test-download-async.c
test_download_async_CFLAGS = -I$(top_srcdir) $(GOBJECT_CFLAGS) $(SOUP_CFLAGS) $(DBUS_GLIB_CFLAGS)
test_download_async_LDADD = $(GOBJECT_LIBS) $(SOUP_LIBS) ../libsocialweb/libsocialweb.la
