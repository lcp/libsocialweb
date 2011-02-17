services_LTLIBRARIES = libmyspace.la
libmyspace_la_SOURCES = module.c \
			myspace.c \
			myspace.h \
			myspace-item-view.h \
			myspace-item-view.c
libmyspace_la_CFLAGS = -I$(top_srcdir) \
		       $(REST_CFLAGS) \
		       $(KEYRING_CFLAGS) \
		       $(DBUS_GLIB_CFLAGS) \
		       $(JSON_GLIB_CFLAGS) \
		       -DG_LOG_DOMAIN=\"MySpace\"
libmyspace_la_LIBADD = $(top_builddir)/libsocialweb/libsocialweb.la \
		       $(top_builddir)/libsocialweb-keystore/libsocialweb-keystore.la \
		       $(top_builddir)/libsocialweb-keyfob/libsocialweb-keyfob.la \
		       $(REST_LIBS) \
		       $(KEYRING_CFLAGS) \
		       $(DBUS_GLIB_LIBS) \
		       $(JSON_GLIB_LIBS)
libmyspace_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = myspace.png

servicesdata_DATA = myspace.keys
CLEANFILES = myspace.keys
EXTRA_DIST = myspace.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
