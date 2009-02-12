services_LTLIBRARIES = liblastfm.la
liblastfm_la_SOURCES = module.c lastfm.c lastfm.h
liblastfm_la_CFLAGS = -I$(top_srcdir) $(REST_CFLAGS) $(GCONF_CFLAGS)
liblastfm_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(REST_LIBS) $(GCONF_LIBS)
liblastfm_la_LDFLAGS = -module -avoid-version
