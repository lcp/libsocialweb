services_LTLIBRARIES = libflickr.la
libflickr_la_SOURCES = module.c \
		       flickr.c flickr.h \
		       flickr-item-view.c flickr-item-view.h
libflickr_la_CFLAGS = -I$(top_srcdir) \
		      $(REST_CFLAGS) \
		      $(GCONF_CFLAGS) \
		      $(SOUP_CFLAGS) \
		      $(DBUS_GLIB_CFLAGS) \
		      -DG_LOG_DOMAIN=\"Flickr\"
libflickr_la_LIBADD = $(top_builddir)/libsocialweb/libsocialweb.la \
		      $(top_builddir)/libsocialweb-keyfob/libsocialweb-keyfob.la \
		      $(top_builddir)/libsocialweb-keystore/libsocialweb-keystore.la \
		      $(REST_LIBS) \
		      $(GCONF_LIBS) \
		      $(SOUP_LIBS) \
		      $(DBUS_GLIB_LIBS)
libflickr_la_LDFLAGS = -module -avoid-version

servicesdata_DATA = flickr.keys
CLEANFILES = flickr.keys
EXTRA_DIST = flickr.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
