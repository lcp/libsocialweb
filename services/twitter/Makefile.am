services_LTLIBRARIES = libtwitter.la
libtwitter_la_SOURCES = module.c twitter.c twitter.h
libtwitter_la_CFLAGS = -I$(top_srcdir) $(TWITTER_CFLAGS) $(GCONF_CFLAGS) $(SOUP_CFLAGS) -DG_LOG_DOMAIN=\"Twitter\"
libtwitter_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(TWITTER_LIBS) $(GCONF_LIBS) $(SOUP_LIBS)
libtwitter_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = twitter.keys twitter.png
