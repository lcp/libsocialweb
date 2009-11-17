lib_LTLIBRARIES = libmojito.la

libmojito_ladir = $(pkgincludedir)/mojito

libmojito_la_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/interfaces \
		     $(DBUS_GLIB_CFLAGS) $(SOUP_CFLAGS) $(SOUP_GNOME_CFLAGS) \
		     $(NM_CFLAGS) $(GTK_CFLAGS) $(REST_CFLAGS) \
		     -DMOJITO_SERVICES_MODULES_DIR=\"$(servicesdir)\"

libmojito_la_LIBADD = $(DBUS_GLIB_LIBS) $(SOUP_LIBS) $(SOUP_GNOME_LIBS) \
		      $(NM_LIBS) $(GTK_LIBS) $(REST_LIBS) \
		      $(top_builddir)/interfaces/libmojito-ginterface.la

libmojito_la_SOURCES = mojito-types.h \
		       mojito-debug.c mojito-debug.h \
		       mojito-core.c mojito-core.h \
		       mojito-item.c mojito-item.h \
		       mojito-view.c mojito-view.h \
		       mojito-item-view.c mojito-item-view.h \
		       mojito-service.c mojito-service.h \
		       mojito-utils.c mojito-utils.h \
		       mojito-web.c mojito-web.h \
		       mojito-set.c mojito-set.h \
		       mojito-cache.c mojito-cache.h \
		       mojito-online.c mojito-online.h \
		       mojito-banned.c mojito-banned.h \
		       mojito-call-list.c mojito-call-list.h \
		       mojito-module.h \
		       client-monitor.c client-monitor.h

libmojito_la_HEADERS = \
	mojito-types.h \
	mojito-service.h \
	mojito-web.h \
	mojito-set.h \
	mojito-item.h \
	mojito-module.h \
	mojito-utils.h

# Test suite
TESTS = test-runner
check_PROGRAMS = test-runner

test_runner_SOURCES = test-runner.c test-runner.h $(libmojito_la_SOURCES) $(top_srcdir)/services/dummy/mojito-service-dummy.c
test_runner_CFLAGS = -DBUILD_TESTS $(libmojito_la_CFLAGS)
test_runner_LDFLAGS = $(libmojito_la_LIBADD)

# TODO: use gtester
