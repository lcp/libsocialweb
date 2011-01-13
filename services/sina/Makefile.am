services_LTLIBRARIES = libsina.la
libsina_la_SOURCES = module.c \
		     sina.c \
		     sina.h \
		     sina-item-view.h \
		     sina-item-view.c
libsina_la_CFLAGS = -I$(top_srcdir) \
		    $(REST_CFLAGS) \
		    $(DBUS_GLIB_CFLAGS) \
		    -DG_LOG_DOMAIN=\"Sina\"
libsina_la_LIBADD = $(top_builddir)/libsocialweb/libsocialweb.la \
		    $(top_builddir)/libsocialweb-keystore/libsocialweb-keystore.la \
		    $(top_builddir)/libsocialweb-keyfob/libsocialweb-keyfob.la \
		    $(REST_LIBS) \
		    $(DBUS_GLIB_LIBS)
libsina_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = sina.png

servicesdata_DATA = sina.keys
CLEANFILES = sina.keys
EXTRA_DIST = sina.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
