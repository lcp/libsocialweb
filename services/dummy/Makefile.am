services_LTLIBRARIES = libmojito-service-dummy.la

libmojito_service_dummy_la_SOURCES = mojito-module-dummy.c \
				 mojito-service-dummy.c mojito-service-dummy.h
libmojito_service_dummy_la_CFLAGS = -I$(top_srcdir) $(GOBJECT_CFLAGS) -DG_LOG_DOMAIN=\"Dummy\"
libmojito_service_dummy_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(GOBJECT_LIBS)
libmojito_service_dummy_la_LDFLAGS = -module -avoid-version
