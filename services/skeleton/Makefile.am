# This should be services_LTLIBRARIES
noinst_LTLIBRARIES = libskeleton.la
libskeleton_la_SOURCES = module.c skeleton.c skeleton.h
libskeleton_la_CFLAGS = -I$(top_srcdir) $(GOBJECT_CFLAGS) -DG_LOG_DOMAIN=\"Skeleton\"
libskeleton_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(GOBJECT_LIBS)
libskeleton_la_LDFLAGS = -module -avoid-version
