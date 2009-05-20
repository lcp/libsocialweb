QUIET_GEN = $(Q:@=@echo '  GEN   '$@;)

lib_LTLIBRARIES = libmojito-keyfob.la
libmojito_keyfob_ladir = $(pkgincludedir)/mojito-keyfob

libmojito_keyfob_la_CFLAGS = -I$(top_srcdir) \
		     $(DBUS_GLIB_CFLAGS) $(KEYRING_CFLAGS) $(REST_CFLAGS)
libmojito_keyfob_la_LIBADD = $(DBUS_GLIB_LIBS) $(KEYRING_LIBS) $(REST_LIBS)

libmojito_keyfob_la_SOURCES = \
	mojito-keyfob.h \
	mojito-keyfob-oauth.c

libmojito_keyfob_la_HEADERS = \
	mojito-keyfob.h

# Test suite
TESTS = test-runner
check_PROGRAMS = test-runner

test_runner_SOURCES = test-runner.c $(libmojito_keyfob_la_SOURCES)
test_runner_CFLAGS = -DBUILD_TESTS $(libmojito_keyfob_la_CFLAGS)
test_runner_LDFLAGS = $(libmojito_keyfob_la_LIBADD)
# TODO: use gtester
