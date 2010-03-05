services_LTLIBRARIES = libdigg.la
libdigg_la_SOURCES = module.c digg.c digg.h
libdigg_la_CFLAGS = -I$(top_srcdir) $(REST_CFLAGS) $(GCONF_CFLAGS) $(DBUS_GLIB_CFLAGS) -DG_LOG_DOMAIN=\"Digg\"
libdigg_la_LIBADD = $(top_builddir)/libsocialweb/libsocialweb.la $(top_builddir)/libsocialweb-keystore/libsocialweb-keystore.la $(REST_LIBS) $(GCONF_LIBS) $(DBUS_GLIB_LIBS)
libdigg_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = digg.keys digg.png

servicesdata_DATA = digg.keys
CLEANFILES = digg.keys
EXTRA_DIST = digg.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
