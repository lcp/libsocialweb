services_LTLIBRARIES = libmojito-service-dummy.la

libmojito_service_dummy_la_SOURCES = mojito-module-dummy.c \
				 mojito-service-dummy.c mojito-service-dummy.h
libmojito_service_dummy_la_CFLAGS = -I$(top_srcdir) \
				   $(GLIB_CFLAGS) \
				   $(SOUP_CFLAGS)
libmojito_service_dummy_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(GLIB_LIBS)
libmojito_service_dummy_la_LDFLAGS = -module -avoid-version
