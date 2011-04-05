services_LTLIBRARIES = libtwitter.la
libtwitter_la_SOURCES = module.c twitter.c twitter.h \
			twitter-contact-view.h twitter-contact-view.c \
			twitter-item-view.h twitter-item-view.c \
			twitter-item-stream.h twitter-item-stream.c
libtwitter_la_CFLAGS = -I$(top_srcdir) $(REST_CFLAGS) \
		       $(JSON_GLIB_CFLAGS) $(DBUS_GLIB_CFLAGS) $(KEYRING_CFLAGS) \
		       -DG_LOG_DOMAIN=\"Twitter\" \
		       $(GCOV_CFLAGS)
libtwitter_la_LIBADD = $(top_builddir)/libsocialweb/libsocialweb.la $(top_builddir)/libsocialweb-keystore/libsocialweb-keystore.la \
		       $(top_builddir)/libsocialweb-keyfob/libsocialweb-keyfob.la \
		       $(REST_LIBS) $(DBUS_GLIB_LIBS) $(KEYRING_LIBS) \
		       $(JSON_GLIB_LIBS) \
		       $(GCOV_LDFLAGS)
libtwitter_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = twitter.png

servicesdata_DATA = twitter.keys
CLEANFILES = twitter.keys
EXTRA_DIST = twitter.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
