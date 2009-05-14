AC_PREREQ(2.53)
AC_INIT(mojito, 0.13.7)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE
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

IT_PROG_INTLTOOL([0.40], [no-xml])
GETTEXT_PACKAGE=mojito
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [The name of the gettext package.])
AC_SUBST(GETTEXT_PACKAGE)
AM_GLIB_GNU_GETTEXT

INTLTOOL_MOJITO_KEYS='%.keys: %.keys.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< [$]@'
AC_SUBST(INTLTOOL_MOJITO_KEYS)

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

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.14)
PKG_CHECK_MODULES(GOBJECT, gobject-2.0 >= 2.14)
PKG_CHECK_MODULES(GCONF, gconf-2.0)
PKG_CHECK_MODULES(SOUP, libsoup-2.4 gthread-2.0)
PKG_CHECK_MODULES(DBUS_GLIB, dbus-glib-1)
PKG_CHECK_MODULES(REST, rest >= 0.3)
PKG_CHECK_MODULES(KEYRING, gnome-keyring-1)


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


AC_MSG_CHECKING([whether to build the Twitter service])
AC_ARG_WITH([twitter],[AS_HELP_STRING([--without-twitter], [disable the Twitter service])],
            [], [with_twitter=yes])
AS_IF(
        [test "$with_twitter" = yes],
        [
        AC_MSG_RESULT([yes])
        PKG_CHECK_MODULES(TWITTER, twitter-glib-1.0 >= 0.9.6)
        ],
        AC_MSG_RESULT([no])
)
AM_CONDITIONAL([WITH_TWITTER], [test x$with_twitter = xyes])


AC_MSG_CHECKING([for MySpace key to use])
AC_ARG_ENABLE([myspace-key], [AS_HELP_STRING([--enable-myspace-key], [API key for MySpace])],
                            [], [enable_myspace_key=no])
AS_IF(
        [test "x$enable_myspace_key" != xno],
        [
        MYSPACE_KEY=`echo $enable_myspace_key | cut -d: -f1`
        MYSPACE_SECRET=`echo $enable_myspace_key | cut -d: -f2`
        if test "$MYSPACE_KEY" -a "$MYSPACE_SECRET" -a "$MYSPACE_KEY" != "$MYSPACE_SECRET"; then
           AC_MSG_RESULT([$MYSPACE_KEY and $MYSPACE_SECRET])
           AC_DEFINE_UNQUOTED([MYSPACE_APIKEY], ["$MYSPACE_KEY"], [MySpace API key])
           AC_DEFINE_UNQUOTED([MYSPACE_SECRET], ["$MYSPACE_SECRET"], [MySpace API secret])
           HAVE_MYSPACE_KEYS=1
        else
           AC_MSG_RESULT([invalid keys, specify key:secret])
        fi
        ],
        AC_MSG_RESULT([not enabling MySpace support])
)
AM_CONDITIONAL([WITH_MYSPACE], [test "$HAVE_MYSPACE_KEYS"])


servicesdir='${libdir}'/mojito/services
AC_SUBST(servicesdir)

servicesdatadir='${pkgdatadir}'/services
AC_SUBST(servicesdatadir)

SHAVE_INIT([build-aux],[enable])

AC_OUTPUT([
        build-aux/shave:m4/shave.in
        build-aux/shave-libtool:m4/shave-libtool.in
        Makefile
        mojito/Makefile
        mojito-keyfob/Makefile
        mojito-keystore/Makefile
        mojito-client/Makefile
        src/Makefile
        services/Makefile
        services/skeleton/Makefile
        services/dummy/Makefile
        services/flickr/Makefile
        services/lastfm/Makefile
        services/twitter/Makefile
        services/myspace/Makefile
        tests/Makefile
        tools/Makefile
        examples/Makefile
        po/Makefile.in
        mojito-client.pc
        mojito-module.pc
        mojito-keystore.pc
])
