ACLOCAL_AMFLAGS = -I m4

SUBDIRS = mojito mojito-client src services examples tests tools

pkgconfig_DATA = mojito-client.pc mojito-module.pc
pkgconfigdir   = $(libdir)/pkgconfig

DISTCHECK_CONFIGURE_FLAGS = --enable-flickr-key=DUMMY --enable-lastfm-key=DUMMY  --enable-myspace-key=DUMMY1:DUMMY2
