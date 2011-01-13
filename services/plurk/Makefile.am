services_LTLIBRARIES = libplurk.la
libplurk_la_SOURCES = module.c \
		      plurk.c \
		      plurk.h \
		      plurk-item-view.h \
		      plurk-item-view.c
libplurk_la_CFLAGS = -I$(top_srcdir) \
		     $(REST_CFLAGS) \
		     $(KEYRING_CFLAGS) \
		     $(DBUS_GLIB_CFLAGS) \
		     $(JSON_GLIB_CFLAGS) \
		     -DG_LOG_DOMAIN=\"Plurk\"
libplurk_la_LIBADD = $(top_builddir)/libsocialweb/libsocialweb.la \
		     $(top_builddir)/libsocialweb-keyfob/libsocialweb-keyfob.la \
		     $(top_builddir)/libsocialweb-keystore/libsocialweb-keystore.la \
		     $(REST_LIBS) \
		     $(KEYRING_LIBS) \
		     $(DBUS_GLIB_LIBS) \
		     $(JSON_GLIB_LIBS)
libplurk_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = plurk.png

servicesdata_DATA = plurk.keys
CLEANFILES = plurk.keys
EXTRA_DIST = plurk.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
