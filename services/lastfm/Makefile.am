QUIET_GEN = $(Q:@=@echo '  GEN   '$@;)

# Common definitions
CLEANFILES =
DISTCLEANFILES =
EXTRA_DIST =
BUILT_SOURCES =

%-ginterface.h %-ginterface.c: %.xml Makefile.am
	$(QUIET_GEN)python $(top_srcdir)/tools/glib-ginterface-gen.py --include='"mojito-marshals.h"' --filename=$(basename $@) $< Mojito_

# glib-genmarshal rules
glib_marshal_list = $(srcdir)/mojito-marshals.list
glib_marshal_prefix = mojito_marshal
include $(top_srcdir)/Makefile.am.marshal

services_LTLIBRARIES = liblastfm.la
liblastfm_la_SOURCES = \
	module.c \
	lastfm.c lastfm.h \
	lastfm-ginterface.c lastfm-ginterface.h \
	mojito-marshals.c mojito-marshals.h
liblastfm_la_CFLAGS = -I$(top_srcdir) $(REST_CFLAGS) $(GCONF_CFLAGS) -DG_LOG_DOMAIN=\"LastFM\"
liblastfm_la_LIBADD = $(top_builddir)/mojito/libmojito.la $(top_builddir)/mojito-keystore/libmojito-keystore.la $(REST_LIBS) $(GCONF_LIBS)
liblastfm_la_LDFLAGS = -module -avoid-version
BUILT_SOURCES += lastfm-ginterface.c lastfm-ginterface.h
CLEANFILES += $(BUILT_SOURCES)

dist_servicesdata_DATA = lastfm.keys lastfm.png

servicesdata_DATA = lastfm.keys
CLEANFILES += lastfm.keys
EXTRA_DIST += lastfm.keys.in
@INTLTOOL_MOJITO_KEYS@
