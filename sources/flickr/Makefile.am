sources_LTLIBRARIES = libflickr.la
libflickr_la_SOURCES = module.c mojito-source-flickr.c mojito-source-flickr.h
libflickr_la_CFLAGS = -I$(top_srcdir)/mojito $(REST_CFLAGS) $(GCONF_CFLAGS) $(SOUP_CFLAGS) -DG_LOG_DOMAIN=\"Flickr\"
libflickr_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(REST_LIBS) $(GCONF_LIBS) $(SOUP_LIBS)
libflickr_la_LDFLAGS = -module -avoid-version
