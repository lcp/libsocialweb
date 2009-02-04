services_LTLIBRARIES = liblastfm.la
liblastfm_la_SOURCES = module.c lastfm.c lastfm.h
liblastfm_la_CFLAGS = -I$(top_srcdir)/mojito $(REST_CFLAGS)
liblastfm_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(REST_LIBS)
liblastfm_la_LDFLAGS = -module -avoid-version
