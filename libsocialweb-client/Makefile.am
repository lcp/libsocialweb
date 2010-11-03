lib_LTLIBRARIES = libsocialweb-client.la

libsocialweb_client_ladir = $(includedir)/libsocialweb/libsocialweb-client
libsocialweb_client_la_CFLAGS = -I$(top_srcdir) $(DBUS_GLIB_CFLAGS) $(GIO_CFLAGS) -DDATADIR="\"$(datadir)\""
libsocialweb_client_la_LIBADD = $(DBUS_GLIB_LIBS) $(GIO_LIBS) $(top_builddir)/interfaces/libsocialweb-bindings.la
libsocialweb_client_la_LDFLAGS = -version-info $(LIBSOCIALWEB_CLIENT_CURRENT):$(LIBSOCIALWEB_CLIENT_REVISION):$(LIBSOCIALWEB_CLIENT_AGE)
libsocialweb_client_la_HEADERS = sw-client.h \
			      sw-item.h sw-client-service.h \
			      sw-client-lastfm.h sw-client-item-view.h
libsocialweb_client_la_SOURCES = sw-client.c \
			      sw-item.c sw-client-service.c \
			      sw-client-service-private.h \
			      sw-client-lastfm.c \
			      sw-client-item-view.c \
			      $(libsocialweb_client_la_HEADERS) \
			      $(BUILT_SOURCES) 
