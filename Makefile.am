ACLOCAL_AMFLAGS = -I m4

SUBDIRS = mojito mojito-keyfob mojito-keystore mojito-client src services examples tests tools po

pkgconfig_DATA = mojito-client.pc mojito-module.pc mojito-keystore.pc
pkgconfigdir   = $(libdir)/pkgconfig

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-flickr-key=DUMMY1:DUMMY2 \
	--enable-lastfm-key=DUMMY \
	--enable-myspace-key=DUMMY1:DUMMY2 \
	--enable-twitter-key=DUMMY1:DUMMY2 \
	--enable-digg-key=DUMMY1

EXTRA_DIST = intltool-extract.in intltool-merge.in intltool-update.in
DISTCLEANFILES = intltool-extract intltool-merge intltool-update
