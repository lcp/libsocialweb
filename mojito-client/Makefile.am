QUIET_GEN = $(Q:@=@echo '  GEN   '$@;)

lib_LTLIBRARIES = libmojito-client.la

%-bindings.h: ../mojito/%.xml
	$(QUIET_GEN)dbus-binding-tool --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

%.c: %.list Makefile.am
	$(QUIET_GEN)glib-genmarshal --body --prefix=mojito_marshal $< > $@

%.h: %.list Makefile.am
	$(QUIET_GEN)glib-genmarshal --header --prefix=mojito_marshal $< > $@

BUILT_SOURCES = mojito-view-bindings.h mojito-core-bindings.h \
		mojito-service-bindings.h \
		mojito-client-marshals.c mojito-client-marshals.h
libmojito_client_ladir = $(includedir)/mojito/mojito-client
libmojito_client_la_CFLAGS = -I$(top_srcdir) $(DBUS_GLIB_CFLAGS)
libmojito_client_la_LIBADD = $(DBUS_GLIB_LIBS)
libmojito_client_la_LDFLAGS = -version-info $(LIBMOJITO_CLIENT_CURRENT):$(LIBMOJITO_CLIENT_REVISION):$(LIBMOJITO_CLIENT_AGE)
libmojito_client_la_HEADERS = mojito-client.h mojito-client-view.h \
			      mojito-item.h mojito-client-service.h 
libmojito_client_la_SOURCES = mojito-client.c mojito-client-view.c \
			      mojito-item.c mojito-client-service.c \
			      mojito-client-service-private.h \
			      $(libmojito_client_la_HEADERS) \
			      $(BUILT_SOURCES) 
