AC_PREREQ(2.59)
AC_INIT(UPnPMPD, 1.1.3, http://www.kitschensync.com/upnpmpd)
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

AM_INIT_AUTOMAKE([1.8 dist-bzip2 no-dist-gzip check-news])

AC_DEFINE(PACKAGE, AC_PACKAGE_NAME, [package name])
AC_DEFINE(VERSION, AC_PACKAGE_VERSION, [package version])

AC_SYS_LARGEFILE
#AC_TYPE_SIZE_T
#AC_TYPE_OFF_T
#AC_DEFINE([_LARGE_FILE_SOURCE], [], [Large files support])
#AC_DEFINE([_FILE_OFFSET_BITS], [64], [File Offset size])


# Checks for programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

EXTRA_GCC_DEBUG_CFLAGS=""
EXTRA_GCC_DEBUG_CXXFLAGS=""

if test -n "$GCC"; then
   EXTRA_GCC_DEBUG_CFLAGS="$CFLAGS"
   EXTRA_GCC_DEBUG_CXXFLAGS="$CXXFLAGS"
   CFLAGS="-Wall -Wpointer-arith -Wcast-align -Wmissing-prototypes -Wmissing-declarations"
   CXXFLAGS="-Wall -Wpointer-arith -Wcast-align"
fi

PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.12 gthread-2.0],,
	                  [AC_MSG_ERROR([GLib 2.12 is required])])
PKG_CHECK_MODULES([MPD], [libmpdclient >= 2.4], HAVE_LIBMPD=yes, HAVE_LIBMPD=no)
PKG_CHECK_MODULES([CFG], [libconfig >= 1.3],,
			  [AC_MSG_ERROR([libconfig is required])])

AC_LIB_UPNP([], [AC_MSG_ERROR([The upnp library is required to build and run this program])])

# Checks for header files.
AC_HEADER_STDC

AC_CONFIG_FILES([Makefile
                 src/Makefile
		 data/Makefile])
AC_OUTPUT

dnl Give error and exit if we don't have libmpdclient
if test "x$HAVE_LIBMPD" = "xno"; then
 AC_MSG_ERROR(you need the MPD Client development packages installed)
fi

