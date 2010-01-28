# SOCIALWEB_ENABLE_SERVICE(DisplayName,optionname,VARNAME)
AC_DEFUN([SOCIALWEB_ENABLE_SERVICE],
[AC_MSG_CHECKING([whether to enable $1])
AC_ARG_ENABLE([$2], [AS_HELP_STRING([--enable-$2], [Enable $1 support])],
                            [], [enable_$2=no])
AS_IF(
        [test "x$enable_$2" != xno],
        [
        AC_MSG_RESULT([$enable_$2])
        ],
        AC_MSG_RESULT([not enabling $1 support])
)
AM_CONDITIONAL([WITH_$3], [test x$enable_$2 != xno])])


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
