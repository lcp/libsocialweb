sources_LTLIBRARIES = libmojito-source-dummy.la

libmojito_source_dummy_la_SOURCES = mojito-module-dummy.c mojito-module-dummy.h \
				 mojito-source-dummy.c mojito-source-dummy.h
libmojito_source_dummy_la_CFLAGS = -I$(top_srcdir)/mojito \
				   $(GLIB_CFLAGS) \
				   $(SOUP_CFLAGS)
libmojito_source_dummy_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(GLIB_LIBS)
libmojito_source_dummy_la_LDFLAGS = -module -avoid-version
