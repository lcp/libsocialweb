lib_LTLIBRARIES = libsocialweb-keystore.la
libsocialweb_keystore_ladir = $(pkgincludedir)/libsocialweb-keystore

libsocialweb_keystore_la_CFLAGS = -I$(top_srcdir) $(GIO_CFLAGS) -DG_LOG_DOMAIN=\"libsocialweb-keystore\"\
				  $(GCOV_CFLAGS)
libsocialweb_keystore_la_LIBADD = $(GIO_LIBS) $(GCOV_LDFLAGS)

libsocialweb_keystore_la_SOURCES = \
	sw-keystore.c sw-keystore.h

libsocialweb_keystore_la_HEADERS = \
	sw-keystore.h


TESTS = test-suite
check_PROGRAMS = test-suite

test_suite_SOURCES = $(libsocialweb_keystore_la_SOURCES)
test_suite_CFLAGS = -DBUILD_TESTS -DSOURCE_DIR=\"$(abs_srcdir)\" $(libsocialweb_keystore_la_CFLAGS)
test_suite_LDFLAGS = $(libsocialweb_keystore_la_LIBADD)
# TODO: use gtester

dist_check_DATA = \
	test-keys/facebook \
	test-keys/flickr \
	test-keys/lastfm \
	test-keys/twitter
