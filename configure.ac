AC_PREREQ(2.53)
AC_INIT(mojito, 0.1)
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

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.14)
PKG_CHECK_MODULES(GCONF, gconf-2.0)
PKG_CHECK_MODULES(SOUP, libsoup-2.4 gthread-2.0)
PKG_CHECK_MODULES(DBUS_GLIB, dbus-glib-1)
PKG_CHECK_MODULES(REST, rest)
PKG_CHECK_MODULES(TWITTER, twitter-glib-1.0)

# Not yet!
#GTK_DOC_CHECK(1.9)

localedir=${datadir}/locale
AC_SUBST(localedir)

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
        services/twitter/Makefile
        examples/Makefile
        mojito-client-0.0.pc
])
