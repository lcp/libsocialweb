sources_LTLIBRARIES = libtwitter.la
libtwitter_la_SOURCES = module.c twitter.c twitter.h
libtwitter_la_CFLAGS = -I$(top_srcdir)/mojito $(SOUP_CFLAGS) -DG_LOG_DOMAIN=\"Twitter\"
libtwitter_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(SOUP_LIBS)
libtwitter_la_LDFLAGS = -module -avoid-version
