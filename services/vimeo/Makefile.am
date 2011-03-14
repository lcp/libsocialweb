services_LTLIBRARIES = libvimeo.la
libvimeo_la_SOURCES = module.c \
		      vimeo.c vimeo.h \
		      vimeo-item-view.c vimeo-item-view.h

libvimeo_la_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/interfaces $(REST_CFLAGS) $(KEYRING_CFLAGS) \
		     $(DBUS_GLIB_CFLAGS) -DG_LOG_DOMAIN=\"Vimeo\" \
		     $(GCOV_CFLAGS)
libvimeo_la_LIBADD = $(top_builddir)/libsocialweb/libsocialweb.la $(top_builddir)/interfaces/libsocialweb-ginterfaces.la \
		     $(top_builddir)/libsocialweb-keystore/libsocialweb-keystore.la \
		     $(top_builddir)/libsocialweb-keyfob/libsocialweb-keyfob.la \
		     $(REST_LIBS) $(KEYRING_LIBS) $(DBUS_GLIB_LIBS) \
		     $(GCOV_LDFLAGS)
libvimeo_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = vimeo.png

servicesdata_DATA = vimeo.keys
CLEANFILES = vimeo.keys
EXTRA_DIST = vimeo.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
