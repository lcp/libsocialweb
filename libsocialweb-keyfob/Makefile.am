lib_LTLIBRARIES = libsocialweb-keyfob.la
libsocialweb_keyfob_ladir = $(pkgincludedir)/libsocialweb-keyfob

libsocialweb_keyfob_la_CFLAGS = -I$(top_srcdir) \
		     $(DBUS_GLIB_CFLAGS) $(KEYRING_CFLAGS) $(REST_CFLAGS)\
		     $(GCOV_CFLAGS)
libsocialweb_keyfob_la_LIBADD = $(DBUS_GLIB_LIBS) $(KEYRING_LIBS) $(REST_LIBS)\
				$(GCOV_LDFLAGS)

libsocialweb_keyfob_la_SOURCES = \
	sw-keyfob.h \
	sw-keyfob-flickr.c \
	sw-keyfob-oauth.c \
	sw-keyfob-oauth2.c

libsocialweb_keyfob_la_HEADERS = \
	sw-keyfob.h

# Test suite
TESTS = test-runner
check_PROGRAMS = test-runner

test_runner_SOURCES = test-runner.c $(libsocialweb_keyfob_la_SOURCES)
test_runner_CFLAGS = -DBUILD_TESTS $(libsocialweb_keyfob_la_CFLAGS)
test_runner_LDFLAGS = $(libsocialweb_keyfob_la_LIBADD)
# TODO: use gtester
