QUIET_GEN = $(Q:@=@echo '  GEN   '$@;)

lib_LTLIBRARIES = libmojito.la

%.c: %.list Makefile.am
	$(QUIET_GEN)glib-genmarshal --body --prefix=mojito_marshal $< > $@

%.h: %.list Makefile.am
	$(QUIET_GEN)glib-genmarshal --header --prefix=mojito_marshal $< > $@

%-ginterface.h %-ginterface.c: %.xml Makefile.am
	$(QUIET_GEN)python $(top_srcdir)/tools/glib-ginterface-gen.py --include='"mojito-marshals.h"' --filename=$(basename $@) $< Mojito_

EXTRA_DIST = mojito-marshals.list mojito-core.xml mojito-service.xml mojito-view.xml

BUILT_SOURCES = \
	mojito-marshals.c mojito-marshals.h \
	mojito-core-ginterface.c mojito-core-ginterface.h \
	mojito-view-ginterface.c mojito-view-ginterface.h \
	mojito-service-ginterface.c mojito-service-ginterface.h

libmojito_ladir = $(pkgincludedir)/mojito

libmojito_la_CFLAGS = -I$(top_srcdir) \
		     $(DBUS_GLIB_CFLAGS) $(SOUP_CFLAGS) $(SOUP_GNOME_CFLAGS) $(NM_CFLAGS) $(GTK_CFLAGS) $(REST_CFLAGS) \
		     -DMOJITO_SERVICES_MODULES_DIR=\"$(servicesdir)\"

libmojito_la_LIBADD = $(DBUS_GLIB_LIBS) $(SOUP_LIBS) $(SOUP_GNOME_LIBS) $(NM_LIBS) $(GTK_LIBS) $(REST_LIBS)

libmojito_la_SOURCES = mojito-types.h \
		       mojito-debug.c mojito-debug.h \
		       mojito-core.c mojito-core.h \
		       mojito-item.c mojito-item.h \
		       mojito-view.c mojito-view.h \
		       mojito-service.c mojito-service.h \
		       mojito-utils.c mojito-utils.h \
		       mojito-web.c mojito-web.h \
		       mojito-set.c mojito-set.h \
		       mojito-cache.c mojito-cache.h \
		       mojito-online.c mojito-online.h \
		       mojito-banned.c mojito-banned.h \
		       mojito-call-list.c mojito-call-list.h \
		       mojito-module.h \
		       client-monitor.c client-monitor.h \
		       $(BUILT_SOURCES)

libmojito_la_HEADERS = \
	mojito-types.h \
	mojito-service.h \
	mojito-web.h \
	mojito-set.h \
	mojito-item.h \
	mojito-module.h \
	mojito-utils.h

CLEANFILES = $(BUILT_SOURCES)

# Test suite
TESTS = test-runner
check_PROGRAMS = test-runner

test_runner_SOURCES = test-runner.c test-runner.h $(libmojito_la_SOURCES) $(top_srcdir)/services/dummy/mojito-service-dummy.c
test_runner_CFLAGS = -DBUILD_TESTS $(libmojito_la_CFLAGS)
test_runner_LDFLAGS = $(libmojito_la_LIBADD)

# TODO: use gtester
