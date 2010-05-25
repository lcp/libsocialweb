QUIET_GEN = $(Q:@=@echo '  GEN   '$@;)

noinst_LTLIBRARIES = libsocialweb-ginterfaces.la libsocialweb-bindings.la

%.c: %.list Makefile.am
	$(QUIET_GEN)glib-genmarshal --body --prefix=sw_marshal $< > $@

%.h: %.list Makefile.am
	$(QUIET_GEN)glib-genmarshal --header --prefix=sw_marshal $< > $@

MARSHALS_SOURCES = \
	sw-marshals.c sw-marshals.h

MARSHALS_LISTS = sw-marshals.list

DBUS_SPECS = sw-core.xml sw-service.xml \
	     sw-item-view.xml sw-query.xml \
	     sw-avatar.xml sw-status-update.xml \
	     lastfm.xml

%-ginterface.h %-ginterface.c: %.xml Makefile.am
	$(QUIET_GEN)python $(top_srcdir)/tools/glib-ginterface-gen.py --include='"sw-marshals.h"' --filename=$(basename $@) $< Sw_

GINTERFACE_SOURCES = \
	$(DBUS_SPECS:.xml=-ginterface.h) \
	$(DBUS_SPECS:.xml=-ginterface.c)

libsocialweb_ginterfaces_la_SOURCES = $(GINTERFACE_SOURCES) $(MARSHALS_SOURCES)
libsocialweb_ginterfaces_la_LIBADD = $(DBUS_GLIB_LIBS)
libsocialweb_ginterfaces_la_CFLAGS = $(DBUS_GLIB_CFLAGS)

libsocialweb_ginterfaces_ladir = $(pkgincludedir)/interfaces
libsocialweb_ginterfaces_la_HEADERS = \
	sw-query-ginterface.h \
	sw-status-update-ginterface.h \
	sw-avatar-ginterface.h

%-bindings.h: %.xml
	$(QUIET_GEN)dbus-binding-tool --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BINDINGS_SOURCES = $(DBUS_SPECS:.xml=-bindings.h)

libsocialweb_bindings_la_SOURCES = $(BINDINGS_SOURCES) $(MARSHALS_SOURCES)
libsocialweb_bindings_la_LIBADD = $(DBUS_GLIB_LIBS)
libsocialweb_bindings_la_CFLAGS = $(DBUS_GLIB_CFLAGS)

BUILT_SOURCES = $(GINTERFACE_SOURCES) $(BINDINGS_SOURCES) $(MARSHALS_SOURCES)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = $(DBUS_SPECS) $(MARSHALS_LISTS)
