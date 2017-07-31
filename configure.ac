
AC_PREREQ(2.59c)

dnl releases only do -Wall, cvs and prerelease does -Werror too
dnl use a three digit version number for releases, and four for cvs/prerelease
AC_INIT(libxice, 0.1.4)
LIBXICE_RELEASE="yes"

AC_CANONICAL_SYSTEM

AC_CONFIG_SRCDIR([agent/agent.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([-Wall])

AC_CONFIG_FILES([
	Makefile
	agent/Makefile
	stun/Makefile
	stun/tests/Makefile
	stun/tools/Makefile
	socket/Makefile
        contexts/Makefile
	xice/Makefile
	xice/xice.pc
	random/Makefile
	docs/Makefile
	docs/reference/Makefile
	docs/reference/libxice/Makefile
	tests/Makefile
	])

# Set the libtool C/A/R version info
#    If the source code was changed, but there were no interface changes:
#      Increment REVISION.
#    If there was a compatible interface change:
#      Increment CURRENT and AGE. Set REVISION to 0
#    If there was an incompatible interface change:
#      Increment CURRENT. Set AGE and REVISION to 0
LIBXICE_CURRENT=12
LIBXICE_REVISION=0
LIBXICE_AGE=2
LIBXICE_LIBVERSION=${LIBXICE_CURRENT}:${LIBXICE_REVISION}:${LIBXICE_AGE}
LIBXICE_LT_LDFLAGS="-version-info ${LIBXICE_LIBVERSION} -no-undefined"
AC_SUBST(LIBXICE_LT_LDFLAGS)


dnl use pretty build output with automake >= 1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],
  [AM_DEFAULT_VERBOSITY=1
   AC_SUBST(AM_DEFAULT_VERBOSITY)])


# Checks for programs.

AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_LIBTOOL

# Check Operating System
AC_MSG_CHECKING([operating system])
case "$host" in
	*-*-*mingw*|*-*-*cygwin*)
		platform=win32
		AC_MSG_RESULT($platform)
		;;
	*)
		platform=linux/other
		AC_MSG_RESULT($platform)
		;;
esac

AM_CONDITIONAL([WINDOWS], [test "$platform" = "win32"])

# Checks for compiler features

AC_C_RESTRICT
AC_C_VARARRAYS
AC_HEADER_ASSERT
AC_HEADER_STDBOOL
AH_VERBATIM([_FORTIFY_SOURCE], 
[/* Define to `2' to get GNU/libc warnings. */
/* Only define if -O1 or more is enabled */
#if defined __OPTIMIZE__ && __OPTIMIZE__ > 0
# define _FORTIFY_SOURCE 2
#endif])
AC_DEFINE([XICEAPI_EXPORT], [ ], [Public library function implementation])
AC_CHECK_HEADERS([arpa/inet.h net/in.h])
AC_CHECK_HEADERS([ifaddrs.h], \
		      [AC_DEFINE(HAVE_GETIFADDRS, [1], \
		       [Whether getifaddrs() is available on the system])])

LIBXICE_CFLAGS="-Wall"
dnl if asked for, add -Werror if supported
if test "x$LIBXICE_RELEASE" != "xyes"; then
  LIBXICE_CFLAGS="$LIBXICE_CFLAGS -Werror"
fi

AC_SUBST(LIBXICE_CFLAGS)
AC_MSG_NOTICE([set LIBXICE_CFLAGS to $LIBXICE_CFLAGS])

# Checks for libraries.
AC_CHECK_LIB(rt, clock_gettime, [LIBRT="-lrt"], [LIBRT=""])
AC_CHECK_FUNCS([poll])
AC_SUBST(LIBRT)

LIBUV_REQUIRED=1.10.0

AC_ARG_ENABLE([libuv],
        AS_HELP_STRING([--disable-libuv],[Disable libuv network support]),
        [case "${enableval}" in
           yes) WANT_LIBUV=yes ;;
           no) WANT_LIBUV=no ;;
           *) AC_MSG_ERROR(bad value ${enableval} for --enable-libuv) ;;
        esac],
        WANT_LIBUV=test)

LIBUV_PACKAGES_PUBLIC=""
LIBUV_PACKAGES_PRIVATE="libuv >= $LIBUV_REQUIRED"
LIBUV_PACKAGES="$LIBUV_PACKAGES_PUBLIC $LIBUV_PACKAGES_PRIVATE"

if test "x$WANT_LIBUV" != "xno"; then
    PKG_CHECK_MODULES(LIBUV, [$LIBUV_PACKAGES],
     [ HAVE_LIBUV=yes ],
     [ HAVE_LIBUV=no ])
fi
if test "x$WANT_LIBUV" = "xyes" && test "x$HAVE_LIBUV" = "xno"; then
  AC_MSG_ERROR([Requested libuv, but it is not available])
fi

if test "x$HAVE_LIBUV" = "xyes"; then
  AC_DEFINE(HAVE_LIBUV,,[Have libuv library])
  LIBUV_ENABLED="true"
else
  LIBUV_PACKAGES_PUBLIC=""
  LIBUV_PACKAGES_PRIVATE=""
  LIBUV_PACKAGES=""
fi

AC_SUBST([LIBUV_PACKAGES_PUBLIC])
AC_SUBST([LIBUV_PACKAGES_PRIVATE])
AC_SUBST([LIBUV_PACKAGES])

AC_SUBST([HAVE_LIBUV])
AC_SUBST([LIBUV_ENABLED])

PKG_CHECK_MODULES(GLIB, [dnl
	glib-2.0 >= 2.13 dnl
	gobject-2.0 >= 2.13 dnl
	gthread-2.0 >= 2.13 dnl
	gio-2.0 >= 2.13 dnl
	])

dnl Test coverage
AC_ARG_ENABLE([coverage],
	[AS_HELP_STRING([--enable-coverage],
		[build for test coverage (default disabled)])],,
	[enable_coverage="no"])
AS_IF([test "${enable_coverage}" != "no"], [
	CFLAGS="${CFLAGS} -g -O0 -fprofile-arcs -ftest-coverage"
	LDFLAGS="-lgcov"
	CCACHE_DISABLE=1
])
AC_SUBST(CCACHE_DISABLE)

case $host_os in
  solaris*)
    LDFLAGS="$LDFLAGS -lsocket -lnls"
    ;;
  *)
    ;;
esac

# check for gtk-doc
m4_ifdef([GTK_DOC_CHECK], [
GTK_DOC_CHECK(1.9)
],[
AM_CONDITIONAL([ENABLE_GTK_DOC], false)
])
AC_CONFIG_MACRO_DIR(m4)

AC_OUTPUT

