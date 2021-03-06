noinst_LTLIBRARIES = libsocialweb-ginterfaces.la libsocialweb-bindings.la

%.c: %.list Makefile.am
	$(AM_V_GEN)glib-genmarshal --body --prefix=sw_marshal $< > $@

%.h: %.list Makefile.am
	$(AM_V_GEN)glib-genmarshal --header --prefix=sw_marshal $< > $@

MARSHALS_SOURCES = \
	sw-marshals.c sw-marshals.h

MARSHALS_LISTS = sw-marshals.list

DBUS_SPECS = sw-core.xml sw-service.xml \
	     sw-item-view.xml sw-contact-view.xml \
	     sw-query.xml sw-contacts-query.xml \
	     sw-avatar.xml sw-status-update.xml \
	     sw-photo-upload.xml sw-banishable.xml \
	     sw-video-upload.xml lastfm.xml \
	     sw-collections.xml

%-ginterface.h %-ginterface.c: %.xml Makefile.am
	$(AM_V_GEN)python $(top_srcdir)/tools/glib-ginterface-gen.py --include='"sw-marshals.h"' --filename=$(basename $@) $< Sw_

GINTERFACE_SOURCES = \
	$(DBUS_SPECS:.xml=-ginterface.h) \
	$(DBUS_SPECS:.xml=-ginterface.c)

libsocialweb_ginterfaces_la_SOURCES = $(GINTERFACE_SOURCES) $(MARSHALS_SOURCES)
libsocialweb_ginterfaces_la_LIBADD = $(DBUS_GLIB_LIBS) $(GCOV_LIBS)
libsocialweb_ginterfaces_la_CFLAGS = $(DBUS_GLIB_CFLAGS) $(GCOV_CFLAGS)

libsocialweb_ginterfaces_ladir = $(pkgincludedir)/interfaces
libsocialweb_ginterfaces_la_HEADERS = \
	sw-query-ginterface.h \
	sw-contacts-query-ginterface.h \
	sw-status-update-ginterface.h \
	sw-avatar-ginterface.h \
	sw-photo-upload-ginterface.h \
	sw-banishable-ginterface.h \
	sw-video-upload-ginterface.h \
	sw-collections-ginterface.h

%-bindings.h: %.xml
	$(AM_V_GEN)dbus-binding-tool --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BINDINGS_SOURCES = $(DBUS_SPECS:.xml=-bindings.h)

libsocialweb_bindings_la_SOURCES = $(BINDINGS_SOURCES) $(MARSHALS_SOURCES)
libsocialweb_bindings_la_LIBADD = $(DBUS_GLIB_LIBS)
libsocialweb_bindings_la_CFLAGS = $(DBUS_GLIB_CFLAGS)

BUILT_SOURCES = $(GINTERFACE_SOURCES) $(BINDINGS_SOURCES) $(MARSHALS_SOURCES)
CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = $(DBUS_SPECS) $(MARSHALS_LISTS)
