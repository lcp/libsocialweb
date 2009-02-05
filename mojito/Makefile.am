lib_LTLIBRARIES = libmojito.la

%.c: %.list Makefile.am
	glib-genmarshal --body --prefix=mojito_marshal $< > $@

%.h: %.list Makefile.am
	glib-genmarshal --header --prefix=mojito_marshal $< > $@

TELEPATHY_GLIB ?= $(top_srcdir)/../telepathy-glib/
%-ginterface.c: %.xml Makefile.am
	python $(TELEPATHY_GLIB)/tools/glib-ginterface-gen.py --include='"mojito-marshals.h"' --filename=$(basename $@) $< Mojito_

BUILT_SOURCES = mojito-marshals.c mojito-marshals.h

libmojito_la_CFLAGS = -I$(top_srcdir) \
		     $(DBUS_GLIB_CFLAGS) $(SOUP_CFLAGS) \
		     -DMOJITO_SERVICES_MODULES_DIR=\"$(servicesdir)\"
libmojito_la_LIBADD = $(DBUS_GLIB_LIBS) $(SOUP_LIBS)
libmojito_la_SOURCES = mojito-core.c mojito-core.h \
		       mojito-item.c mojito-item.h \
		       mojito-view.c mojito-view.h \
		       mojito-service.c mojito-service.h \
		       mojito-utils.c mojito-utils.h \
		       mojito-web.c mojito-web.h \
		       mojito-set.c mojito-set.h \
		       mojito-cache.c mojito-cache.h \
		       mojito-module.h \
		       client-monitor.c client-monitor.h \
		       $(BUILT_SOURCES) \
		       mojito-core-ginterface.c mojito-core-ginterface.h \
		       mojito-view-ginterface.c mojito-view-ginterface.h
