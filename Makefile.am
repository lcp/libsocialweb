ACLOCAL_AMFLAGS = -I m4

SUBDIRS = interfaces libsocialweb libsocialweb-keyfob libsocialweb-keystore libsocialweb-client src services examples tests tools po

pkgconfig_DATA = libsocialweb-client.pc libsocialweb-module.pc libsocialweb-keystore.pc
pkgconfigdir   = $(libdir)/pkgconfig

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-flickr \
	--enable-lastfm \
	--enable-myspace \
	--enable-twitter \
	--enable-digg

EXTRA_DIST = intltool-extract.in intltool-merge.in intltool-update.in
DISTCLEANFILES = intltool-extract intltool-merge intltool-update
