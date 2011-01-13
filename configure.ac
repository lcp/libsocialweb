m4_define([api_major], [0])
m4_define([api_minor], [25])
m4_define([api_micro], [12])
m4_define([sw_version], [api_major.api_minor.api_micro])

AC_PREREQ([2.63])
AC_INIT([libsocialweb], [sw_version],
        [],
        [libsocialweb],
        [http://meego.gitorious.org/meego-middleware/libsocialweb])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_HOST

AC_GNU_SOURCE
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_ISC_POSIX
AC_HEADER_STDC

AM_INIT_AUTOMAKE([1.11 foreign -Wno-portability])
AM_SILENT_RULES([yes])

# require libtool >= 2.2
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

# Libtool versioning
# See for details: http://bit.ly/Y5oX
AC_SUBST(LIBSOCIALWEB_CLIENT_CURRENT, [2])
AC_SUBST(LIBSOCIALWEB_CLIENT_REVISION, [0])
AC_SUBST(LIBSOCIALWEB_CLIENT_AGE, [0])

AM_PROG_CC_C_O

# TODO: bit nasty, should we use gnome-common?
CFLAGS="$CFLAGS -Wall"

AM_PATH_GLIB_2_0([2.14.0],
                 [],
                 [AC_MSG_ERROR([glib-2.0 is required])],
                 [gobject gthread gmodule-no-export])

AS_ALL_LINGUAS
IT_PROG_INTLTOOL([0.40], [no-xml])
GETTEXT_PACKAGE=libsocialweb
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [The name of the gettext package.])
AC_SUBST(GETTEXT_PACKAGE)
AM_GLIB_GNU_GETTEXT

INTLTOOL_SOCIALWEB_KEYS='%.keys: %.keys.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< [$]@'
AC_SUBST(INTLTOOL_SOCIALWEB_KEYS)

GTK_DOC_CHECK([1.15],[--flavour no-tmpl])

# only check if gtk-doc is enabled
AS_IF([test "x$enable_gtk_doc" = "xyes"], [AC_CHECK_PROGS(XSLT, [xsltproc])])

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.14)
PKG_CHECK_MODULES(GIO, gio-2.0)
PKG_CHECK_MODULES(GOBJECT, gobject-2.0 >= 2.14)
PKG_CHECK_MODULES(GCONF, gconf-2.0)
PKG_CHECK_MODULES(SOUP, libsoup-2.4 gthread-2.0)
PKG_CHECK_MODULES(DBUS_GLIB, dbus-glib-1)
PKG_CHECK_MODULES(REST, rest-0.7 rest-extras-0.7 >= 0.7.1)
PKG_CHECK_MODULES(KEYRING, gnome-keyring-1)
PKG_CHECK_MODULES(JSON_GLIB, json-glib-1.0)

AC_MSG_CHECKING([how to detect we are online])
AC_ARG_WITH([online],
            [AS_HELP_STRING([--with-online],
                            [how to check if we are online (always, connman, networkmanager, test)])],
                            [],
                            [with_online=always])

AS_CASE(["$with_online"],

        ["always"],
        [
        AC_MSG_RESULT([always])
        AC_DEFINE([WITH_ONLINE_ALWAYS], 1, [No online detection])
        ],

        ["connman"],
        [
        AC_MSG_RESULT([Connection Manager])
        AC_DEFINE([WITH_ONLINE_CONNMAN], 1, [ConnMan online detection])
        ],

        ["networkmanager"],
        [
        AC_MSG_RESULT([Network Manager])
        PKG_CHECK_MODULES(NM, libnm-glib >= 0.7)
        AC_DEFINE([WITH_ONLINE_NM], 1, [NM online detection])
        ],

        ["test"],
        [
        AC_MSG_RESULT([test UI])
        PKG_CHECK_MODULES(GTK, gtk+-2.0)
        AC_DEFINE([WITH_ONLINE_TEST], 1, [Test UI online detection])
        ],

        [AC_MSG_ERROR([Unknown argument "$with_online" to --with-online])]
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

AC_ARG_ENABLE([all-services],[AS_HELP_STRING([--enable-all-services], [enable every service])],
              [], [enable_all_services=no])

SOCIALWEB_ENABLE_SERVICE(Facebook, facebook, FACEBOOK)
SOCIALWEB_ENABLE_SERVICE(Flickr, flickr, FLICKR)
SOCIALWEB_ENABLE_SERVICE(Last.fm, lastfm, LASTFM)
SOCIALWEB_ENABLE_SERVICE(Twitter, twitter, TWITTER)
SOCIALWEB_ENABLE_SERVICE(Vimeo, vimeo, VIMEO)
SOCIALWEB_ENABLE_SERVICE(SumMug, smugmug, SMUGMUG)
SOCIALWEB_ENABLE_SERVICE(Plurk, plurk, PLURK)

servicesdir='${libdir}'/libsocialweb/services
AC_SUBST(servicesdir)

servicesdatadir='${datadir}'/libsocialweb/services
AC_SUBST(servicesdatadir)


dnl === Coverage report =======================================================
AC_PATH_PROG([GCOV], [lcov], [enable_gcov=no])

AC_MSG_CHECKING([whether to build with gcov testing])

AC_ARG_ENABLE([gcov],
              [AS_HELP_STRING([--enable-gcov],
                              [Whether to enable coverage testing (requires gcc
and lcov)])],
              [],
              [enable_gcov=no])

AS_IF([test "x$enable_gcov" = "xyes" && test "x$GCC" = "xyes"],
      [
        AS_IF([test "x$enable_gtk_doc" = "xyes"],
              [AC_MSG_WARN([gtk-doc is enabled, this may go horribly wrong])],
              [AC_MSG_RESULT([yes])]
        )

        GCOV_CFLAGS="-g -O0 -fprofile-arcs -ftest-coverage"
        GCOV_LDFLAGS="-lgcov"
      ],
      [AC_MSG_RESULT([no])]
)

AM_CONDITIONAL([GCOV_ENABLED], [test "x$enable_gcov" = "xyes"])
AC_SUBST([GCOV_CFLAGS])
AC_SUBST([GCOV_LDFLAGS])

AC_OUTPUT([
        Makefile
        docs/Makefile
        docs/libsocialweb/Makefile
        docs/libsocialweb-client/Makefile
        docs/libsocialweb-dbus/Makefile
        libsocialweb/Makefile
        interfaces/Makefile
        libsocialweb-keyfob/Makefile
        libsocialweb-keystore/Makefile
        libsocialweb-client/Makefile
        src/Makefile
        services/Makefile
        services/dummy/Makefile
        services/facebook/Makefile
        services/flickr/Makefile
        services/lastfm/Makefile
        services/plurk/Makefile
	services/smugmug/Makefile
        services/twitter/Makefile
        services/vimeo/Makefile
        tests/Makefile
        tools/Makefile
        examples/Makefile
        po/Makefile.in
        libsocialweb-client.pc
        libsocialweb-module.pc
        libsocialweb-keyfob.pc
        libsocialweb-keystore.pc
])
