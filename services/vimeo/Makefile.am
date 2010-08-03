services_LTLIBRARIES = libvimeo.la
libvimeo_la_SOURCES = module.c \
		      vimeo.c vimeo.h \
		      vimeo-item-view.c vimeo-item-view.h

libvimeo_la_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/interfaces $(REST_CFLAGS) $(GCONF_CFLAGS) $(DBUS_GLIB_CFLAGS) -DG_LOG_DOMAIN=\"Vimeo\"
libvimeo_la_LIBADD = $(top_builddir)/libsocialweb/libsocialweb.la $(top_builddir)/interfaces/libsocialweb-ginterfaces.la $(REST_LIBS) $(GCONF_LIBS) $(DBUS_GLIB_LIBS)
libvimeo_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = vimeo.png

servicesdata_DATA = vimeo.keys
CLEANFILES = vimeo.keys
EXTRA_DIST = vimeo.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
