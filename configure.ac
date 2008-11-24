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
PKG_CHECK_MODULES(SOUP, libsoup-2.4)
PKG_CHECK_MODULES(DBUS_GLIB, dbus-glib-1)
PKG_CHECK_MODULES(REST, rest)
PKG_CHECK_MODULES(SQLITE, sqlite3)
#PKG_CHECK_MODULES(RSS_GLIB, rss-glib-1.0)

# Not yet!
#GTK_DOC_CHECK(1.9)

localedir=${datadir}/locale
AC_SUBST(localedir)

sourcesdir='${libdir}'/mojito/sources
AC_SUBST(sourcesdir)


AC_OUTPUT([
        Makefile
        mojito/Makefile
        src/Makefile
        sources/Makefile
        sources/dummy/Makefile
])
