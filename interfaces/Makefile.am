QUIET_GEN = $(Q:@=@echo '  GEN   '$@;)

noinst_LTLIBRARIES = libmojito-ginterfaces.la

%.c: %.list Makefile.am
	$(QUIET_GEN)glib-genmarshal --body --prefix=mojito_marshal $< > $@

%.h: %.list Makefile.am
	$(QUIET_GEN)glib-genmarshal --header --prefix=mojito_marshal $< > $@

MARSHALS_SOURCES = \
	mojito-marshals.c mojito-marshals.h

MARSHALS_LISTS = mojito-marshals.list

GINTERFACE_SOURCES = \
	mojito-core-ginterface.c mojito-core-ginterface.h \
	mojito-view-ginterface.c mojito-view-ginterface.h \
	mojito-service-ginterface.c mojito-service-ginterface.h \
	mojito-item-view-ginterface.c mojito-item-view-ginterface.h \
	mojito-query-ginterface.c mojito-query-ginterface.h \
	mojito-avatar-ginterface.c mojito-avatar-ginterface.h \
	mojito-status-update-ginterface.c mojito-status-update-ginterface.h

DBUS_SPECS = mojito-core.xml mojito-service.xml \
	     mojito-view.xml mojito-item-view.xml mojito-query.xml \
	     mojito-avatar.xml mojito-status-update.xml

%-ginterface.h %-ginterface.c: %.xml Makefile.am
	$(QUIET_GEN)python $(top_srcdir)/tools/glib-ginterface-gen.py --include='"mojito-marshals.h"' --filename=$(basename $@) $< Mojito_

libmojito_ginterfaces_la_SOURCES = $(GINTERFACE_SOURCES) $(MARSHALS_SOURCES)
libmojito_ginterfaces_la_LIBADD = $(DBUS_GLIB_LIBS)
libmojito_ginterfaces_la_CFLAGS = $(DBUS_GLIB_CFLAGS)

BUILT_SOURCES = $(GINTERFACE_SOURCES) $(MARSHALS_SOURCES)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = $(DBUS_SPECS) $(MARSHALS_LISTS)
