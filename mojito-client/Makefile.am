QUIET_GEN = $(Q:@=@echo '  GEN   '$@;)

lib_LTLIBRARIES = libmojito-client.la

libmojito_client_ladir = $(includedir)/mojito/mojito-client
libmojito_client_la_CFLAGS = -I$(top_srcdir) $(DBUS_GLIB_CFLAGS)
libmojito_client_la_LIBADD = $(DBUS_GLIB_LIBS) $(top_builddir)/interfaces/libmojito-bindings.la
libmojito_client_la_LDFLAGS = -version-info $(LIBMOJITO_CLIENT_CURRENT):$(LIBMOJITO_CLIENT_REVISION):$(LIBMOJITO_CLIENT_AGE)
libmojito_client_la_HEADERS = mojito-client.h mojito-client-view.h \
			      mojito-item.h mojito-client-service.h \
			      mojito-client-lastfm.h mojito-client-item-view.h
libmojito_client_la_SOURCES = mojito-client.c mojito-client-view.c \
			      mojito-item.c mojito-client-service.c \
			      mojito-client-service-private.h \
			      mojito-client-lastfm.c \
			      mojito-client-item-view.c \
			      $(libmojito_client_la_HEADERS) \
			      $(BUILT_SOURCES) 
