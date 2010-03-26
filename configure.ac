AC_PREREQ(2.53)
AC_INIT(libsocialweb, 0.24.3)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign -Wno-portability])
AM_CONFIG_HEADER(config.h)

AC_CANONICAL_HOST

AC_GNU_SOURCE
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_ISC_POSIX
AC_HEADER_STDC
AM_PROG_CC_C_O

AS_ALL_LINGUAS
IT_PROG_INTLTOOL([0.40], [no-xml])
GETTEXT_PACKAGE=libsocialweb
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [The name of the gettext package.])
AC_SUBST(GETTEXT_PACKAGE)
AM_GLIB_GNU_GETTEXT

AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])

INTLTOOL_SOCIALWEB_KEYS='%.keys: %.keys.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< [$]@'
AC_SUBST(INTLTOOL_SOCIALWEB_KEYS)

# TODO: bit nasty, should we use gnome-common?
CFLAGS="$CFLAGS -Wall"

# Libtool versioning
# See for details: http://bit.ly/Y5oX
LIBSOCIALWEB_CLIENT_CURRENT=1
LIBSOCIALWEB_CLIENT_REVISION=0
LIBSOCIALWEB_CLIENT_AGE=0

AC_SUBST(LIBSOCIALWEB_CLIENT_CURRENT)
AC_SUBST(LIBSOCIALWEB_CLIENT_REVISION)
AC_SUBST(LIBSOCIALWEB_CLIENT_AGE)

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.14)
PKG_CHECK_MODULES(GIO, gio-2.0)
PKG_CHECK_MODULES(GOBJECT, gobject-2.0 >= 2.14)
PKG_CHECK_MODULES(GCONF, gconf-2.0)
PKG_CHECK_MODULES(SOUP, libsoup-2.4 gthread-2.0)
PKG_CHECK_MODULES(DBUS_GLIB, dbus-glib-1)
PKG_CHECK_MODULES(REST, rest-0.6 rest-extras-0.6)
PKG_CHECK_MODULES(KEYRING, gnome-keyring-1)


AC_MSG_CHECKING([how to detect we are online])
AC_ARG_WITH([online],
        [AS_HELP_STRING([--with-online],
                [how to check if we are online (always, connman, networkmanager, test)])],
        [], [with_online=always])
AS_IF(
        [test "$with_online" = always],
        [
        AC_MSG_RESULT([always])
        AC_DEFINE([WITH_ONLINE_ALWAYS], 1, [No online detection])
        ],

        [test "$with_online" = connman],
        [
        AC_MSG_RESULT([Connection Manager])
        AC_DEFINE([WITH_ONLINE_CONNMAN], 1, [ConnMan online detection])
        ],

        [test "$with_online" = networkmanager],
        [
        AC_MSG_RESULT([Network Manager])
        PKG_CHECK_MODULES(NM, libnm-glib >= 0.7)
        AC_DEFINE([WITH_ONLINE_NM], 1, [NM online detection])
        ],

        [test "$with_online" = "test"],
        [
        AC_MSG_RESULT([test UI])
        PKG_CHECK_MODULES(GTK, gtk+-2.0)
        AC_DEFINE([WITH_ONLINE_TEST], 1, [Test UI online detection])
        ],

        [AC_MSG_ERROR([Unknown argument to --with-online])]
)


AC_MSG_CHECKING([whether to use the GNOME environment])
AC_ARG_WITH([gnome],[AS_HELP_STRING([--without-gnome], [disable support for GNOME environment])],
            [], [with_gnome=yes])
AS_IF(
        [test "$with_gnome" = yes],
        [
        AC_MSG_RESULT([yes])
        AC_DEFINE([WITH_GNOME], 1, [Use GNOME])
        PKG_CHECK_MODULES(SOUP_GNOME, libsoup-gnome-2.4 >= 2.25.1)
        ],
        AC_MSG_RESULT([no])
)


SOCIALWEB_ENABLE_SERVICE(Digg, digg, DIGG)
SOCIALWEB_ENABLE_SERVICE(Flickr, flickr, FLICKR)
SOCIALWEB_ENABLE_SERVICE(Last.fm, lastfm, LASTFM)
SOCIALWEB_ENABLE_SERVICE(MySpace, myspace, MYSPACE)
SOCIALWEB_ENABLE_SERVICE(Twitter, twitter, TWITTER)


servicesdir='${libdir}'/libsocialweb/services
AC_SUBST(servicesdir)

servicesdatadir='${pkgdatadir}'/services
AC_SUBST(servicesdatadir)

SHAVE_INIT([build-aux],[enable])

AC_OUTPUT([
        build-aux/shave:m4/shave.in
        build-aux/shave-libtool:m4/shave-libtool.in
        Makefile
        libsocialweb/Makefile
        interfaces/Makefile
        libsocialweb-keyfob/Makefile
        libsocialweb-keystore/Makefile
        libsocialweb-client/Makefile
        src/Makefile
        services/Makefile
        services/dummy/Makefile
        services/flickr/Makefile
        services/lastfm/Makefile
        services/twitter/Makefile
        services/myspace/Makefile
        services/digg/Makefile
        tests/Makefile
        tools/Makefile
        examples/Makefile
        po/Makefile.in
        libsocialweb-client.pc
        libsocialweb-module.pc
        libsocialweb-keystore.pc
])
