ACLOCAL_AMFLAGS = -I m4

SUBDIRS = interfaces mojito mojito-keyfob mojito-keystore mojito-client src services examples tests tools po

pkgconfig_DATA = mojito-client.pc mojito-module.pc mojito-keystore.pc
pkgconfigdir   = $(libdir)/pkgconfig

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-flickr \
	--enable-lastfm \
	--enable-myspace \
	--enable-twitter \
	--enable-digg

EXTRA_DIST = intltool-extract.in intltool-merge.in intltool-update.in
DISTCLEANFILES = intltool-extract intltool-merge intltool-update
