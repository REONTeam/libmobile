AC_PREREQ([2.71])
LT_PREREQ([2.4.6])
AC_INIT([libmobile], [0.2.1])
AM_INIT_AUTOMAKE([1.16.5 -Wall foreign])
AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_AR
LT_INIT([win32-dll])
PKG_INSTALLDIR

EXTRA_CFLAGS=""
AC_SUBST([EXTRA_CFLAGS])

AC_DEFUN([MY_FEATURE_ENABLE], [dnl
AC_ARG_ENABLE([$1], AS_HELP_STRING([--enable-$1], [$3]),
    AS_IF([test "$enableval" = yes], AC_DEFINE([$2])))dnl
])dnl

# Feature definitions
MY_FEATURE_ENABLE([impl-weak], [MOBILE_ENABLE_IMPL_WEAK],
    [use weak implementation callbacks])
MY_FEATURE_ENABLE([noalloc], [MOBILE_ENABLE_NOALLOC],
    [disable functions for memory allocation])

# Default cflags
AS_IF([test "$GCC" = yes], [dnl
    EXTRA_CFLAGS="$EXTRA_CFLAGS -std=c11 -Wall -Wextra"])

AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_HEADERS([autoconf.h mobile_config.h])
AC_CONFIG_FILES([
    Makefile
    libmobile.pc
])

AC_OUTPUT
