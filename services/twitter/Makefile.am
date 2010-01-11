services_LTLIBRARIES = libtwitter.la
libtwitter_la_SOURCES = module.c twitter.c twitter.h \
			twitter-item-view.h twitter-item-view.c
libtwitter_la_CFLAGS = -I$(top_srcdir) $(REST_CFLAGS) $(GCONF_CFLAGS) $(DBUS_GLIB_CFLAGS) -DG_LOG_DOMAIN=\"Twitter\"
libtwitter_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(top_builddir)/mojito-keystore/libmojito-keystore.la $(top_builddir)/mojito-keyfob/libmojito-keyfob.la $(REST_LIBS) $(GCONF_LIBS) $(DBUS_GLIB_LIBS)
libtwitter_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = twitter.png

servicesdata_DATA = twitter.keys
CLEANFILES = twitter.keys
EXTRA_DIST = twitter.keys.in
@INTLTOOL_MOJITO_KEYS@
