AC_PREREQ(2.53)
AC_INIT(mojito, 0.10.1)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(config.h)

AC_CANONICAL_HOST

AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_ISC_POSIX
AC_HEADER_STDC
AM_PROG_CC_C_O

# TODO: bit nasty, should we use gnome-common?
CFLAGS="$CFLAGS -Wall"

# Libtool versioning
# See for details: http://bit.ly/Y5oX
LIBMOJITO_CLIENT_CURRENT=1
LIBMOJITO_CLIENT_REVISION=0
LIBMOJITO_CLIENT_AGE=0

AC_SUBST(LIBMOJITO_CLIENT_CURRENT)
AC_SUBST(LIBMOJITO_CLIENT_REVISION)
AC_SUBST(LIBMOJITO_CLIENT_AGE)

PKG_CHECK_MODULES(GOBJECT, gobject-2.0 >= 2.14)
PKG_CHECK_MODULES(GCONF, gconf-2.0)
PKG_CHECK_MODULES(SOUP, libsoup-2.4 gthread-2.0)
PKG_CHECK_MODULES(DBUS_GLIB, dbus-glib-1)
PKG_CHECK_MODULES(REST, rest)
PKG_CHECK_MODULES(TWITTER, twitter-glib-1.0 >= 0.9.6)

AC_MSG_CHECKING([how to detect we are online])
AC_ARG_WITH([online],
        [AS_HELP_STRING([--with-online],
                [how to check if we are online (always, connman, networkmanager)])],
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
        PKG_CHECK_MODULES(NM, NetworkManager)
        AC_DEFINE([WITH_ONLINE_NM], 1, [NM online detection])
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

# TODO: make this a m4 function in acinclude
AC_MSG_CHECKING([for Flickr key to use])
AC_ARG_ENABLE([flickr-key], [AS_HELP_STRING([--enable-flickr-key], [API key for Flickr])],
                            [], [enable_flickr_key=no])
AS_IF(
        [test "x$enable_flickr_key" != xno],
        [
        AC_MSG_RESULT([$enable_flickr_key])
        AC_DEFINE_UNQUOTED([FLICKR_APIKEY], ["$enable_flickr_key"], [Flickr API key])
        ],
        AC_MSG_RESULT([not enabling Flickr support])
)
AM_CONDITIONAL([WITH_FLICKR], [test x$enable_flickr_key != xno])


AC_MSG_CHECKING([for Lastfm key to use])
AC_ARG_ENABLE([lastfm-key], [AS_HELP_STRING([--enable-lastfm-key], [API key for Lastfm])],
                            [], [enable_lastfm_key=no])
AS_IF(
        [test "x$enable_lastfm_key" != xno],
        [
        AC_MSG_RESULT([$enable_lastfm_key])
        AC_DEFINE_UNQUOTED([LASTFM_APIKEY], ["$enable_lastfm_key"], [Lastfm API key])
        ],
        AC_MSG_RESULT([not enabling Lastfm support])
)
AM_CONDITIONAL([WITH_LASTFM], [test x$enable_lastfm_key != xno])


servicesdir='${libdir}'/mojito/services
AC_SUBST(servicesdir)


AC_OUTPUT([
        Makefile
        mojito/Makefile
        mojito-client/Makefile
        src/Makefile
        services/Makefile
        services/dummy/Makefile
        services/flickr/Makefile
        services/lastfm/Makefile
        services/twitter/Makefile
        tests/Makefile
        tools/Makefile
        examples/Makefile
        mojito-client.pc
        mojito-module.pc
])
