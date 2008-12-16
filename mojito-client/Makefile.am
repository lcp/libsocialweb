lib_LTLIBRARIES = libmojito-client.la

%-bindings.h: ../mojito/%.xml
	dbus-binding-tool --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

%.c: %.list Makefile.am
	glib-genmarshal --body --prefix=mojito_marshal $< > $@

%.h: %.list Makefile.am
	glib-genmarshal --header --prefix=mojito_marshal $< > $@

BUILT_SOURCES = mojito-view-bindings.h mojito-core-bindings.h \
		mojito-client-marshals.c mojito-client-marshals.h

libmojito_client_la_CFLAGS = $(DBUS_GLIB_CFLAGS)
libmojito_client_la_LIBADD = $(DBUS_GLIB_LIBS)
libmojito_client_la_SOURCES = mojito-client.h mojito-client.c \
			      mojito-client-view.h mojito-client-view.c \
			      mojito-item.h mojito-item.c \
			      $(BUILT_SOURCES) 
