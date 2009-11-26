QUIET_GEN = $(Q:@=@echo '  GEN   '$@;)

noinst_LTLIBRARIES = libmojito-ginterfaces.la libmojito-bindings.la

%.c: %.list Makefile.am
	$(QUIET_GEN)glib-genmarshal --body --prefix=mojito_marshal $< > $@

%.h: %.list Makefile.am
	$(QUIET_GEN)glib-genmarshal --header --prefix=mojito_marshal $< > $@

MARSHALS_SOURCES = \
	mojito-marshals.c mojito-marshals.h

MARSHALS_LISTS = mojito-marshals.list

DBUS_SPECS = mojito-core.xml mojito-service.xml \
	     mojito-view.xml mojito-item-view.xml mojito-query.xml \
	     mojito-avatar.xml mojito-status-update.xml \
	     lastfm.xml

%-ginterface.h %-ginterface.c: %.xml Makefile.am
	$(QUIET_GEN)python $(top_srcdir)/tools/glib-ginterface-gen.py --include='"mojito-marshals.h"' --filename=$(basename $@) $< Mojito_

GINTERFACE_SOURCES = \
	$(DBUS_SPECS:.xml=-ginterface.h) \
	$(DBUS_SPECS:.xml=-ginterface.c)

libmojito_ginterfaces_la_SOURCES = $(GINTERFACE_SOURCES) $(MARSHALS_SOURCES)
libmojito_ginterfaces_la_LIBADD = $(DBUS_GLIB_LIBS)
libmojito_ginterfaces_la_CFLAGS = $(DBUS_GLIB_CFLAGS)

%-bindings.h: %.xml
	$(QUIET_GEN)dbus-binding-tool --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BINDINGS_SOURCES = $(DBUS_SPECS:.xml=-bindings.h)

libmojito_bindings_la_SOURCES = $(BINDINGS_SOURCES) $(MARSHALS_SOURCES)
libmojito_bindings_la_LIBADD = $(DBUS_GLIB_LIBS)
libmojito_bindings_la_CFLAGS = $(DBUS_GLIB_CFLAGS)

BUILT_SOURCES = $(GINTERFACE_SOURCES) $(BINDINGS_SOURCES) $(MARSHALS_SOURCES)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = $(DBUS_SPECS) $(MARSHALS_LISTS)
