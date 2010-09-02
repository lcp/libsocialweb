services_LTLIBRARIES = libtwitter.la
libtwitter_la_SOURCES = module.c twitter.c twitter.h \
			twitter-item-view.h twitter-item-view.c
libtwitter_la_CFLAGS = -I$(top_srcdir) $(REST_CFLAGS) $(GCONF_CFLAGS) $(DBUS_GLIB_CFLAGS) -DG_LOG_DOMAIN=\"Twitter\" \
		       $(GCOV_CFLAGS)
libtwitter_la_LIBADD = $(top_builddir)/libsocialweb/libsocialweb.la $(top_builddir)/libsocialweb-keystore/libsocialweb-keystore.la \
		       $(top_builddir)/libsocialweb-keyfob/libsocialweb-keyfob.la \
		       $(REST_LIBS) $(GCONF_LIBS) $(DBUS_GLIB_LIBS) \
		       $(GCOV_LDFLAGS)
libtwitter_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = twitter.png

servicesdata_DATA = twitter.keys
CLEANFILES = twitter.keys
EXTRA_DIST = twitter.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
