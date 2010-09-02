services_LTLIBRARIES = liblastfm.la
liblastfm_la_SOURCES = module.c \
		       lastfm.c lastfm.h \
		       lastfm-item-view.c lastfm-item-view.h

liblastfm_la_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/interfaces $(REST_CFLAGS) \
		      $(GCONF_CFLAGS) $(DBUS_GLIB_CFLAGS) -DG_LOG_DOMAIN=\"LastFM\" \
		      $(GCOV_CFLAGS)
liblastfm_la_LIBADD = $(top_builddir)/libsocialweb/libsocialweb.la $(top_builddir)/libsocialweb-keystore/libsocialweb-keystore.la \
		      $(top_builddir)/interfaces/libsocialweb-ginterfaces.la $(REST_LIBS) $(GCONF_LIBS) $(DBUS_GLIB_LIBS) \
		      $(GCOV_LDFLAGS)
liblastfm_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = lastfm.png

servicesdata_DATA = lastfm.keys
CLEANFILES = lastfm.keys
EXTRA_DIST = lastfm.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
