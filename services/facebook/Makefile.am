services_LTLIBRARIES = libfacebook.la

libfacebook_la_SOURCES = \
	module.c \
	facebook.c facebook.h \
	facebook-util.c facebook-util.h \
	facebook-contact-view.c facebook-contact-view.h \
	facebook-item-view.c facebook-item-view.h

libfacebook_la_CFLAGS = \
	-I$(top_srcdir) \
	$(REST_CFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	-DG_LOG_DOMAIN=\"Facebook\"

libfacebook_la_LIBADD = \
	$(top_builddir)/libsocialweb/libsocialweb.la \
	$(top_builddir)/libsocialweb-keyfob/libsocialweb-keyfob.la \
	$(top_builddir)/libsocialweb-keystore/libsocialweb-keystore.la \
	$(REST_LIBS) \
	$(JSON_GLIB_LIBS) \
	$(DBUS_GLIB_CFLAGS)

libfacebook_la_LDFLAGS = \
	-module \
	-avoid-version

servicesdata_DATA = facebook.keys
@INTLTOOL_SOCIALWEB_KEYS@

CLEANFILES = facebook.keys
EXTRA_DIST = facebook.keys.in
