dnl $Id$
dnl
dnl Copyright (c) 2003,2003
dnl         The XFce development team. All rights reserved.
dnl
dnl Written for XFce by Benedikt Meurer <benny@xfce.org>.
dnl

dnl Version information
m4_define([xfsm_version_major], [4])
m4_define([xfsm_version_minor], [1])
m4_define([xfsm_version_micro], [2])
m4_define([xfsm_version], [xfsm_version_major.xfsm_version_minor.xfsm_version_micro])

dnl Initialize autoconf
AC_COPYRIGHT([Copyright (c) 2003,2004
        The XFce development team. All rights reserved.
        
Written for XFce by Benedikt Meurer <benny@xfce.org>.])
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
AC_CHECK_FUNCS([mkdir clearenv sigaction])
AC_CHECK_HEADERS([errno.h fcntl.h limits.h signal.h sys/param.h sys/utsname.h \
  sys/wait.h time.h unistd.h])

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
BM_I18N([xfce4-session], [ar az ca de es es_MX eu fa_IR fr hi it ja ko ms nl no
	 pl pt_BR sv ta tr zh_CN zh_TW])

dnl Check for required packages
BM_DEPEND([LIBXFCE4UTIL], [libxfce4util-1.0], [4.1.1])
BM_DEPEND([LIBXFCE4MCS_CLIENT], [libxfce4mcs-client-1.0], [4.1.0])
BM_DEPEND([LIBXFCEGUI4], [libxfcegui4-1.0], [4.1.0])

dnl Configure the MCS plugins
XFCE_MCS_PLUGIN([XFCE_MCS_MANAGER], [4.1.0])

dnl Check for debugging support
BM_DEBUG_SUPPORT

dnl
dnl arguments to set shutdown commands
dnl

AC_MSG_CHECKING([for custom 'poweroff' command])
AC_ARG_WITH([poweroff],
	AC_HELP_STRING([--with-poweroff=cmd], [command used to power down (full path with arguments)]),
	[with_poweroff="$withval"],
	[with_poweroff="no"])
if test "x$with_poweroff" != "xno"; then
	AC_DEFINE_UNQUOTED(POWEROFF_CMD, "$with_poweroff", [poweroff command])
fi
AC_MSG_RESULT([$with_poweroff])

AC_MSG_CHECKING([for custom 'halt' command])
AC_ARG_WITH([halt],
	AC_HELP_STRING([--with-halt=cmd], [command used to halt (full path with arguments)]),
	[with_halt="$withval"],
	[with_halt="no"])
if test "x$with_halt" != "xno"; then
	AC_DEFINE_UNQUOTED(HALT_CMD, "$with_halt", [halt command])
fi
AC_MSG_RESULT([$with_halt])

AC_MSG_CHECKING([for custom 'reboot' command])
AC_ARG_WITH([reboot],
	AC_HELP_STRING([--with-reboot=cmd], [command used to reboot (full path with arguments)]),
	[with_reboot="$withval"],
	[with_reboot="no"])
if test "x$with_reboot" != "xno"; then
	AC_DEFINE_UNQUOTED(REBOOT_CMD, "$with_reboot", [reboot command])
fi
AC_MSG_RESULT([$with_reboot])

AC_OUTPUT([
Makefile
defaults/Makefile
docs/Makefile
settings/Makefile
po/Makefile.in
xfce4-session.spec
xfce4-session/Makefile
xfce4-shutdown/Makefile
])
