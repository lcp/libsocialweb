AC_PREREQ(2.53)
AC_INIT(mojito, 0.2)
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

# Libtool versioning
# See for details: http://bit.ly/Y5oX
LIBMOJITO_CLIENT_CURRENT=1
LIBMOJITO_CLIENT_REVISION=0
LIBMOJITO_CLIENT_AGE=0

AC_SUBST(LIBMOJITO_CLIENT_CURRENT)
AC_SUBST(LIBMOJITO_CLIENT_REVISION)
AC_SUBST(LIBMOJITO_CLIENT_AGE)

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.14)
PKG_CHECK_MODULES(GCONF, gconf-2.0)
PKG_CHECK_MODULES(SOUP, libsoup-2.4 gthread-2.0)
PKG_CHECK_MODULES(DBUS_GLIB, dbus-glib-1)
PKG_CHECK_MODULES(REST, rest)
PKG_CHECK_MODULES(TWITTER, twitter-glib-1.0 >= 0.9.2)

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
        examples/Makefile
        mojito-client.pc
])
