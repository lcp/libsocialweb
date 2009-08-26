# MOJITO_OAUTH_KEYS(DisplayName,optionname,VAR_NAME)
AC_DEFUN([MOJITO_OAUTH_KEYS],
[AC_MSG_CHECKING([for $1 key to use])
AC_ARG_ENABLE([$2-key], [AS_HELP_STRING([--enable-$2-key], [API key for $1])],
                            [], [enable_$2_key=no])
AS_IF(
        [test "x$enable_$2_key" != xno],
        [
        _oauth_key=`echo $enable_$2_key | cut -d: -f1`
        _oauth_secret=`echo $enable_$2_key | cut -d: -f2`
        if test "$_oauth_key" -a "$_oauth_secret" -a "$_oauth_key" != "$_oauth_secret"; then
           AC_MSG_RESULT([$_oauth_key and $_oauth_secret])
           AC_DEFINE_UNQUOTED([$3_APIKEY], ["$_oauth_key"], [$1 API key])
           AC_DEFINE_UNQUOTED([$3_SECRET], ["$_oauth_secret"], [$1 API secret])
           HAVE_$3_KEYS=1
        else
           AC_MSG_RESULT([invalid keys, specify key:secret])
        fi
        ],
        AC_MSG_RESULT([not enabling $1 support])
)
AM_CONDITIONAL([WITH_$3], [test "$HAVE_$3_KEYS"])])


# Set ALL_LINGUAS based on the .po files present. Optional argument is the name
# of the po directory.
AC_DEFUN([AS_ALL_LINGUAS],
[
 AC_MSG_CHECKING([for linguas])
 podir="m4_default([$1],[$srcdir/po])"
 ALL_LINGUAS=`cd $podir && ls *.po 2>/dev/null | awk 'BEGIN { FS="."; ORS=" " } { print $[]1 }'`
 AC_SUBST([ALL_LINGUAS])
 AC_MSG_RESULT($ALL_LINGUAS)
])
