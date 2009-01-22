lib_LTLIBRARIES = libmojito-client.la

%-bindings.h: ../mojito/%.xml
	dbus-binding-tool --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

%.c: %.list Makefile.am
	glib-genmarshal --body --prefix=mojito_marshal $< > $@

%.h: %.list Makefile.am
	glib-genmarshal --header --prefix=mojito_marshal $< > $@

BUILT_SOURCES = mojito-view-bindings.h mojito-core-bindings.h \
		mojito-client-marshals.c mojito-client-marshals.h
libmojito_client_ladir = $(includedir)/mojito/mojito-client
libmojito_client_la_CFLAGS = -I$(top_srcdir) $(DBUS_GLIB_CFLAGS)
libmojito_client_la_LIBADD = $(DBUS_GLIB_LIBS)
libmojito_client_la_HEADERS = mojito-client.h mojito-client-view.h \
			      mojito-item.h
libmojito_client_la_SOURCES = mojito-client.c mojito-client-view.c \
			      mojito-item.c \
			      $(libmojito_client_la_HEADERS) \
			      $(BUILT_SOURCES) 
