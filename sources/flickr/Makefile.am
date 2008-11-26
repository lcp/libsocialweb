sources_LTLIBRARIES = libflickr.la
libflickr_la_SOURCES = mojito-source-flickr.c mojito-source-flickr.h
libflickr_la_CFLAGS = -I$(top_srcdir)/mojito $(REST_CFLAGS)
libflickr_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(REST_LIBS)
libflickr_la_LDFLAGS = -module -avoid-version
