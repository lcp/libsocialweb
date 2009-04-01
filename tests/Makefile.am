noinst_PROGRAMS = test-online test-client-online test-download

test_online_SOURCES = test-online.c
test_online_CFLAGS = -I$(top_srcdir) $(GOBJECT_CFLAGS)
test_online_LDADD = $(GOBJECT_LIBS) ../mojito/libmojito.la

test_client_online_SOURCES = test-client-online.c
test_client_online_CFLAGS = -I$(top_srcdir) $(GOBJECT_CFLAGS)
test_client_online_LDADD = $(GOBJECT_LIBS) ../mojito-client/libmojito-client.la

test_download_SOURCES = test-download.c
test_download_CFLAGS = -I$(top_srcdir) $(GOBJECT_CFLAGS) $(SOUP_CFLAGS)
test_download_LDADD = $(GOBJECT_LIBS) $(SOUP_LIBS) ../mojito/libmojito.la
