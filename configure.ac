dnl configure.ac
dnl
dnl Copyright (c) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
dnl

AC_INIT([configure.ac])

AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([xfce4-session], [0.0.21])

AM_MAINTAINER_MODE

dnl check for UNIX variants
AC_AIX
AC_ISC_POSIX
AC_MINIX

dnl check for basic programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

dnl check for standard header files
AC_HEADER_STDC
AC_CHECK_FUNCS([gethostname setenv])
AC_CHECK_HEADERS([errno.h fcntl.h limits.h sys/param.h sys/utsname.h \
  sys/wait.h time.h])

dnl Check for X11 installed
BM_LIBX11_REQUIRE

dnl Check for session management support
BM_LIBSM

dnl Check for i18n support
BM_I18N([xfce4-session], [de es es_MX nl])

dnl Check for required packages
BM_DEPEND([LIBXFCE4UTIL], [libxfce4util-1.0], [3.91.0])
BM_DEPEND([LIBXFCE4MCS_CLIENT], [libxfce4mcs-client-1.0], [3.91.0])
BM_DEPEND([LIBXFCEGUI4], [libxfcegui4-1.0], [3.92.1])

dnl Configure the MCS plugins
XFCE_MCS_PLUGIN([XFCE_MCS_MANAGER], [3.91.0])

dnl Check for debugging support
BM_DEBUG_SUPPORT

AC_OUTPUT([
Makefile
defaults/Makefile
defaults/splash/Makefile
docs/Makefile
settings/Makefile
po/Makefile.in
xfce4-session/Makefile
xfce4-shutdown/Makefile
])
