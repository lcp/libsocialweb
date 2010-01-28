noinst_LTLIBRARIES = libdummy.la

libdummy_la_SOURCES = module.c dummy.c dummy.h
libdummy_la_CFLAGS = -I$(top_srcdir) $(GOBJECT_CFLAGS) -DG_LOG_DOMAIN=\"Dummy\"
libdummy_la_LIBADD = $(top_builddir)/libsocialweb/libsocialweb.la $(GOBJECT_LIBS)
libdummy_la_LDFLAGS = -module -avoid-version
