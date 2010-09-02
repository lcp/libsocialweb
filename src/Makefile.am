libexec_PROGRAMS = libsocialweb-core

libsocialweb_core_CFLAGS = -I$(top_srcdir) $(DBUS_GLIB_CFLAGS) $(SOUP_CFLAGS) $(GCOV_CFLAGS)
libsocialweb_core_SOURCES = main.c poll.h poll.c
libsocialweb_core_LDADD = ../libsocialweb/libsocialweb.la $(DBUS_GLIB_LIBS) $(SOUP_LIBS) $(GCOV_LDFLAGS)

%.service: %.service.in ../config.log
	$(AM_V_GEN)sed -e "s|\@dir\@|$(libexecdir)|" $< > $@

servicedir = $(datadir)/dbus-1/services/
service_in_files = libsocialweb.service.in
service_DATA = libsocialweb.service

CLEANFILES = libsocialweb.service

EXTRA_DIST = $(service_in_files)
