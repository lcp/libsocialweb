lib_LTLIBRARIES = libmojito.la

%-glue.h: %.xml
	dbus-binding-tool --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

%-bindings.h: %.xml
	dbus-binding-tool --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

%.c: %.list
	glib-genmarshal --body --prefix=mojito_marshal $^ > $@

%.h: %.list
	glib-genmarshal --header --prefix=mojito_marshal $^ > $@

TELEPATHY_GLIB ?= $(top_srcdir)/../telepathy-glib/
%-ginterface.c: %.xml
	python $(TELEPATHY_GLIB)/tools/glib-ginterface-gen.py --include='"mojito-marshals.h"' --filename=$(basename $@) $^ Mojito_

BUILT_SOURCES = mojito-marshals.c mojito-marshals.h

libmojito_la_CFLAGS = $(DBUS_GLIB_CFLAGS) $(SQLITE_CFLAGS) $(SOUP_CFLAGS) \
		     -DMOJITO_SOURCES_MODULES_DIR=\"$(sourcesdir)\"
libmojito_la_LIBADD = $(DBUS_GLIB_LIBS) $(SQLITE_LIBS) $(SOUP_LIBS)
libmojito_la_SOURCES = mojito-core.c mojito-core.h \
		       mojito-view.c mojito-view.h \
		       mojito-source.c mojito-source.h \
		       mojito-utils.c mojito-utils.h \
		       mojito-item.c mojito-item.h \
		       mojito-sqlite.c mojito-sqlite.h \
		       mojito-web.c mojito-web.h \
		       $(BUILT_SOURCES) \
		       mojito-core-ginterface.c mojito-core-ginterface.h \
		       mojito-view-ginterface.c mojito-view-ginterface.h
