CLEANFILES =

lib_LTLIBRARIES = libsocialweb-client.la

libsocialweb_client_ladir = $(includedir)/libsocialweb/libsocialweb-client
libsocialweb_client_la_CFLAGS = -I$(top_srcdir) $(DBUS_GLIB_CFLAGS) $(GIO_CFLAGS) -DDATADIR="\"$(datadir)\""
libsocialweb_client_la_LIBADD = $(DBUS_GLIB_LIBS) $(GIO_LIBS) $(top_builddir)/interfaces/libsocialweb-bindings.la
libsocialweb_client_la_LDFLAGS = -version-info $(LIBSOCIALWEB_CLIENT_CURRENT):$(LIBSOCIALWEB_CLIENT_REVISION):$(LIBSOCIALWEB_CLIENT_AGE)
HANDWRITTED_H_SOURCES = sw-client.h \
			      sw-item.h sw-client-service.h \
			      sw-client-lastfm.h sw-client-item-view.h
libsocialweb_client_la_HEADERS = $(HANDWRITTED_H_SOURCES)
HANDWRITTED_C_SOURCES = sw-client.c \
			      sw-item.c sw-client-service.c \
			      sw-client-lastfm.c \
			      sw-client-item-view.c
PRIVATE_H_SOURCES = sw-client-service-private.h
libsocialweb_client_la_SOURCES = $(HANDWRITTED_C_SOURCES) \
			      $(PRIVATE_H_SOURCES) \
			      $(libsocialweb_client_la_HEADERS) \
			      $(BUILT_SOURCES) 

if HAVE_INTROSPECTION

if HAVE_VALA

libsocialweb_client_la_HEADERS += libsocialweb-client-hack-for-vala.h

libsocialweb-client-hack-for-vala.h: $(HANDWRITTED_H_SOURCES) \
    libsocialweb-client-hack-for-vala.h.header.in \
    libsocialweb-client-hack-for-vala.h.footer.in
	$(AM_V_GEN)( \
	    cat libsocialweb-client-hack-for-vala.h.header.in; \
	    for include in $(HANDWRITTED_H_SOURCES); do \
		echo "#include <libsocialweb-client/$$include>"; \
	    done; \
	    cat libsocialweb-client-hack-for-vala.h.footer.in \
	) > $@

endif # if HAVE_VALA

INTROSPECTION_FILES = $(HANDWRITTED_C_SOURCES) \
		      $(HANDWRITTED_H_SOURCES)

SocialWebClient-0.25.gir: $(INTROSPECTION_SCANNER) \
		$(INTROSPECTION_FILES) \
		kludge.xsl \
		libsocialweb-client.la
	$(AM_V_GEN)$(INTROSPECTION_SCANNER) -v \
		--namespace SocialWebClient \
		--nsversion=0.25 \
		--identifier-prefix=Sw \
		-I$(top_builddir) \
		-I$(top_srcdir) \
		--include=GLib-2.0 \
		--include=Gio-2.0 \
		--libtool="$(top_builddir)/libtool" \
		--library=libsocialweb-client.la \
		--warn-all \
		--output $@.tmp \
		$(INTROSPECTION_FILES)
	@xsltproc kludge.xsl $@.tmp > $@
	@rm -f $@.tmp

girdir = $(datadir)/gir-1.0
gir_DATA = SocialWebClient-0.25.gir

%.typelib: %.gir $(INTROSPECTION_COMPILER)
	$(AM_V_GEN)$(top_builddir)/libtool --mode=execute \
	    $(INTROSPECTION_COMPILER) \
	        --includedir=$(srcdir) \
	        --includedir=. \
	        $(INTROSPECTION_COMPILER_OPTS) $< -o $(@F)

typelibsdir = $(libdir)/girepository-1.0/
typelibs_DATA = $(gir_DATA:.gir=.typelib)

CLEANFILES += \
    $(gir_DATA) \
    $(typelibs_DATA)

endif # if HAVE_INTROSPECTION

EXTRA_DIST = \
	     libsocialweb-client-hack-for-vala.h.header.in \
	     libsocialweb-client-hack-for-vala.h.footer.in
