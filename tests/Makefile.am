noinst_PROGRAMS = test-online test-client-online

test_online_SOURCES = test-online.c
test_online_CFLAGS = -I$(top_srcdir) $(GOBJECT_CFLAGS)
test_online_LDADD = $(GOBJECT_CFLAGS) ../mojito/libmojito.la

test_client_online_SOURCES = test-client-online.c
test_client_online_CFLAGS = -I$(top_srcdir) $(GOBJECT_CFLAGS)
test_client_online_LDADD = $(GOBJECT_CFLAGS) ../mojito-client/libmojito-client.la
