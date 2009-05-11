services_LTLIBRARIES = libmyspace.la
libmyspace_la_SOURCES = module.c myspace.c myspace.h
libmyspace_la_CFLAGS = -I$(top_srcdir) $(REST_CFLAGS) -DG_LOG_DOMAIN=\"MySpace\"
libmyspace_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(top_builddir)/mojito-keyfob/libmojito-keyfob.la $(REST_LIBS)
libmyspace_la_LDFLAGS = -module -avoid-version $(top_builddir)/mojito-keyfob/libmojito-keyfob.la

dist_servicesdata_DATA = myspace.png

servicesdata_DATA = myspace.keys
CLEANFILES = myspace.keys
EXTRA_DIST = myspace.keys.in
@INTLTOOL_MOJITO_KEYS@
