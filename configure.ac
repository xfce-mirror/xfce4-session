dnl $Id$
dnl
dnl Copyright (c) 2003-2004
dnl         The Xfce development team. All rights reserved.
dnl
dnl Written for Xfce by Benedikt Meurer <benny@xfce.org>.
dnl

dnl Version information
m4_define([xfsm_version_major], [4])
m4_define([xfsm_version_minor], [1])
m4_define([xfsm_version_micro], [10])
m4_define([xfsm_version], [xfsm_version_major.xfsm_version_minor.xfsm_version_micro])

dnl Initialize autoconf
AC_COPYRIGHT([Copyright (c) 2003-2004
        The Xfce development team. All rights reserved.
        
Written for Xfce by Benedikt Meurer <benny@xfce.org>.])
AC_INIT([xfce4-session], [xfsm_version], [xfce4-dev@xfce.org])
AC_PREREQ([2.50])
AC_REVISION([$Id$])

dnl Initialize automake
XFSM_VERSION=xfsm_version
AM_INIT_AUTOMAKE([xfce4-session], [$XFSM_VERSION])
AM_CONFIG_HEADER([config.h])
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
AC_CHECK_FUNCS([getaddrinfo gethostbyname gethostname sigaction strdup sync])
AC_CHECK_HEADERS([errno.h fcntl.h limits.h netdb.h signal.h stdarg.h \
  sys/param.h sys/resource.h sys/socket.h sys/time.h sys/wait.h sys/utsname.h \
  time.h unistd.h])

dnl Check for X11 installed
BM_LIBX11_REQUIRE

dnl Check for session management support
BM_LIBSM

dnl Check for _IceTransNoListen
ac_LIBS="$LIBS"
LIBS="$LIBS $LIBSM_LDFLAGS $LIBSM_LIBS"
AC_CHECK_FUNCS([_IceTransNoListen])
LIBS="$ac_LIBS"

dnl Check for i18n support
BM_I18N([xfce4-session], [ar az ca de es es_MX eu fa_IR fr hi it ja ko lt ms nl no
	 pl pt_BR sv ta tr zh_CN zh_TW])

dnl Check for required packages
BM_DEPEND([LIBXFCE4UTIL], [libxfce4util-1.0], [4.1.11])
BM_DEPEND([LIBXFCE4MCS_CLIENT], [libxfce4mcs-client-1.0], [4.1.0])
BM_DEPEND([LIBXFCEGUI4], [libxfcegui4-1.0], [4.1.20])

dnl Configure the MCS plugins
XFCE_MCS_PLUGIN([XFCE_MCS_MANAGER], [4.1.0])

dnl Check for gnome support
BM_DEPEND_CHECK([GNOME], [libgnome-2.0], [2.4.0], [gnome],
  [Gnome support], [yes])

dnl Check for debugging support
BM_DEBUG_SUPPORT

dnl Check for iceauth
AC_PATH_PROG([ICEAUTH], [iceauth])
if test x"$ICEAUTH" != x""; then
  AC_DEFINE_UNQUOTED([ICEAUTH_CMD], ["$ICEAUTH"], [path to iceauth])
else
  AC_MSG_ERROR([iceauth missing, please check your X11 installation])
fi

dnl Check for sudo
AC_PATH_PROG([SUDO], [sudo])
if test x"$SUDO" != x""; then
  AC_DEFINE_UNQUOTED([SUDO_CMD], ["$SUDO"], [path to sudo])
fi

dnl Check for rm
AC_PATH_PROG([RM], [rm])
if test x"$RM" != x""; then
  AC_DEFINE_UNQUOTED([RM_CMD], ["$RM"], [path to rm])
fi

dnl Check for legacy session management
AC_ARG_ENABLE([legacy-sm],
AC_HELP_STRING([--enable-legacy-sm], [Enable X11R5 session management])
AC_HELP_STRING([--disable-legacy-sm], [Disable X11R5 session management]),
  [], [enable_legacy_sm=yes])
AC_MSG_CHECKING([whether to enable X11R5 session management])
if test x"$enable_legacy_sm" != x"yes"; then
  AC_MSG_RESULT([no])
else
  AC_DEFINE([LEGACY_SESSION_MANAGEMENT], [1], [Define for X11R5 sm compat])
  AC_MSG_RESULT([yes])
fi

dnl Check whether to create session screenshots
AC_ARG_ENABLE([session-screenshots],
AC_HELP_STRING([--enable-session-screenshots], [Create screenshots of sessions on logout]),
  [], [enable_session_screenshots=no])
AC_MSG_CHECKING([whether to create screenshots of sessions on logout])
if test x"$enable_session_screenshots" != x"yes"; then
  AC_MSG_RESULT([no])
else
  AC_DEFINE([SESSION_SCREENSHOTS], [1], [Define for session screenshots])
  AC_MSG_RESULT([yes])
fi

dnl
dnl arguments to set shutdown commands
dnl

dnl AC_MSG_CHECKING([for custom 'poweroff' command])
dnl AC_ARG_WITH([poweroff],
dnl   AC_HELP_STRING([--with-poweroff=cmd], [command used to power down (full path with arguments)]),
dnl   [with_poweroff="$withval"],
dnl   [with_poweroff="no"])
dnl if test "x$with_poweroff" != "xno"; then
dnl   AC_DEFINE_UNQUOTED(POWEROFF_CMD, "$with_poweroff", [poweroff command])
dnl fi
dnl AC_MSG_RESULT([$with_poweroff])
dnl 
dnl AC_MSG_CHECKING([for custom 'halt' command])
dnl AC_ARG_WITH([halt],
dnl   AC_HELP_STRING([--with-halt=cmd], [command used to halt (full path with arguments)]),
dnl   [with_halt="$withval"],
dnl   [with_halt="no"])
dnl if test "x$with_halt" != "xno"; then
dnl   AC_DEFINE_UNQUOTED(HALT_CMD, "$with_halt", [halt command])
dnl fi
dnl AC_MSG_RESULT([$with_halt])
dnl 
dnl AC_MSG_CHECKING([for custom 'reboot' command])
dnl AC_ARG_WITH([reboot],
dnl   AC_HELP_STRING([--with-reboot=cmd], [command used to reboot (full path with arguments)]),
dnl   [with_reboot="$withval"],
dnl   [with_reboot="no"])
dnl if test "x$with_reboot" != "xno"; then
dnl   AC_DEFINE_UNQUOTED(REBOOT_CMD, "$with_reboot", [reboot command])
dnl fi
dnl AC_MSG_RESULT([$with_reboot])

AC_OUTPUT([
Makefile
xfce4-session.spec
defaults/Makefile
docs/Makefile
icons/Makefile
icons/48x48/Makefile
icons/64x64/Makefile
icons/72x72/Makefile
icons/128x128/Makefile
po/Makefile.in
settings/Makefile
settings/scripts/Makefile
themes/Makefile
themes/Default/Makefile
xfce4-session/Makefile
xfsm-shutdown-helper/Makefile
])
