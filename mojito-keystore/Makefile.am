QUIET_GEN = $(Q:@=@echo '  GEN   '$@;)

lib_LTLIBRARIES = libmojito-keystore.la
libmojito_keystore_ladir = $(pkgincludedir)/mojito-keystore

libmojito_keystore_la_CFLAGS = -I$(top_srcdir) $(GIO_CFLAGS) -DG_LOG_DOMAIN=\"Mojito-KeyStore\"
libmojito_keystore_la_LIBADD = $(GIO_LIBS)

libmojito_keystore_la_SOURCES = \
	mojito-keystore.c mojito-keystore.h

libmojito_keystore_la_HEADERS = \
	mojito-keystore.h


TESTS = test-suite
check_PROGRAMS = test-suite

test_suite_SOURCES = $(libmojito_keystore_la_SOURCES)
test_suite_CFLAGS = -DBUILD_TESTS -DSOURCE_DIR=\"$(abs_srcdir)\" $(libmojito_keystore_la_CFLAGS)
test_suite_LDFLAGS = $(libmojito_keystore_la_LIBADD)
# TODO: use gtester

dist_check_DATA = \
	test-keys/digg \
	test-keys/facebook \
	test-keys/flickr \
	test-keys/lastfm \
	test-keys/myspace \
	test-keys/twitter
