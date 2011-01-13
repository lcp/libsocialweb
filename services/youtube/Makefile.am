services_LTLIBRARIES = libyoutube.la
libyoutube_la_SOURCES = module.c \
			youtube.c \
		        youtube.h \
			youtube-item-view.h \
		        youtube-item-view.c
libyoutube_la_CFLAGS = -I$(top_srcdir) \
		       $(REST_CFLAGS) \
		       $(KEYRING_CFLAGS) \
		       $(DBUS_GLIB_CFLAGS) \
		       -I$(top_srcdir)/utils -DG_LOG_DOMAIN=\"Youtube\"
libyoutube_la_LIBADD = $(top_builddir)/libsocialweb/libsocialweb.la \
		       $(top_builddir)/libsocialweb-keystore/libsocialweb-keystore.la \
		       $(top_builddir)/libsocialweb-keyfob/libsocialweb-keyfob.la \
		       $(REST_LIBS) \
		       $(KEYRING_LIBS) \
		       $(DBUS_GLIB_LIBS)
libyoutube_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = youtube.png

servicesdata_DATA = youtube.keys
CLEANFILES = youtube.keys
EXTRA_DIST = youtube.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
