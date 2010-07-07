CLEANFILES=
DISTCLEANFILES=
BUILT_SOURCES=
EXTRA_DIST=

lib_LTLIBRARIES = libsocialweb.la

libsocialweb_ladir = $(pkgincludedir)/libsocialweb

libsocialweb_la_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/interfaces \
		     $(DBUS_GLIB_CFLAGS) $(SOUP_CFLAGS) $(SOUP_GNOME_CFLAGS) \
		     $(NM_CFLAGS) $(GTK_CFLAGS) $(REST_CFLAGS) \
		     -DSOCIALWEB_SERVICES_MODULES_DIR=\"$(servicesdir)\"

libsocialweb_la_LIBADD = $(DBUS_GLIB_LIBS) $(SOUP_LIBS) $(SOUP_GNOME_LIBS) \
		      $(NM_LIBS) $(GTK_LIBS) $(REST_LIBS) \
		      $(top_builddir)/interfaces/libsocialweb-ginterfaces.la

libsocialweb_la_SOURCES = sw-types.h \
		       sw-debug.c sw-debug.h \
		       sw-core.c sw-core.h \
		       sw-item.c sw-item.h \
		       sw-item-view.c sw-item-view.h \
		       sw-service.c sw-service.h \
		       sw-utils.c sw-utils.h \
		       sw-web.c sw-web.h \
		       sw-set.c sw-set.h \
		       sw-cache.c sw-cache.h \
		       sw-online.c sw-online.h \
		       sw-banned.c sw-banned.h \
		       sw-call-list.c sw-call-list.h \
		       sw-module.h \
		       client-monitor.c client-monitor.h \
		       sw-enum-types.h sw-enum-types.c

libsocialweb_la_HEADERS = \
	sw-core.h \
	sw-types.h \
	sw-service.h \
	sw-online.h \
	sw-item-view.h \
	sw-debug.h \
	sw-web.h \
	sw-set.h \
	sw-item.h \
	sw-module.h \
	sw-utils.h \
	sw-enum-types.h

# Test suite
TESTS = test-runner
check_PROGRAMS = test-runner

test_runner_SOURCES = test-runner.c test-runner.h $(libsocialweb_la_SOURCES) $(top_srcdir)/services/dummy/dummy.c
test_runner_CFLAGS = -DBUILD_TESTS $(libsocialweb_la_CFLAGS)
test_runner_LDFLAGS = $(libsocialweb_la_LIBADD)

glib_enum_h = sw-enum-types.h
glib_enum_c = sw-enum-types.c
glib_enum_headers = $(libsocialweb_la_HEADERS)
include $(top_srcdir)/glib-enums.mk

# TODO: use gtester
