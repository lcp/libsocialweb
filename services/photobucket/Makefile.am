services_LTLIBRARIES = libphotobucket.la
libphotobucket_la_SOURCES = module.c \
		      photobucket.c photobucket.h

libphotobucket_la_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/interfaces \
		     $(SOUP_CFLAGS) $(REST_CFLAGS) $(KEYRING_CFLAGS) \
		     $(DBUS_GLIB_CFLAGS) -DG_LOG_DOMAIN=\"Photobucket\" \
		     $(GCOV_CFLAGS)
libphotobucket_la_LIBADD = $(top_builddir)/libsocialweb/libsocialweb.la \
		     $(top_builddir)/interfaces/libsocialweb-ginterfaces.la \
		     $(top_builddir)/libsocialweb-keystore/libsocialweb-keystore.la \
		     $(top_builddir)/libsocialweb-keyfob/libsocialweb-keyfob.la \
		     $(SOUP_LIBS) $(REST_LIBS) $(KEYRING_LIBS) $(DBUS_GLIB_LIBS) \
		     $(GCOV_LDFLAGS)
libphotobucket_la_LDFLAGS = -module -avoid-version

servicesdata_DATA = photobucket.keys
CLEANFILES = photobucket.keys
EXTRA_DIST = photobucket.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
