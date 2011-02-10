services_LTLIBRARIES = libsmugmug.la
libsmugmug_la_SOURCES = module.c \
		      smugmug.c smugmug.h

libsmugmug_la_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/interfaces \
		     $(REST_CFLAGS) $(KEYRING_CFLAGS) \
		     $(DBUS_GLIB_CFLAGS) -DG_LOG_DOMAIN=\"SmugMug\" \
		     $(GCOV_CFLAGS)
libsmugmug_la_LIBADD = $(top_builddir)/libsocialweb/libsocialweb.la \
		     $(top_builddir)/interfaces/libsocialweb-ginterfaces.la \
		     $(top_builddir)/libsocialweb-keystore/libsocialweb-keystore.la \
		     $(top_builddir)/libsocialweb-keyfob/libsocialweb-keyfob.la \
		     $(REST_LIBS) $(KEYRING_LIBS) $(DBUS_GLIB_LIBS) \
		     $(GCOV_LDFLAGS)
libsmugmug_la_LDFLAGS = -module -avoid-version

servicesdata_DATA = smugmug.keys
CLEANFILES = smugmug.keys
EXTRA_DIST = smugmug.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
