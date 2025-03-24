dnl
dnl Copyright (c) 2003-2024
dnl         The Xfce development team. All rights reserved.
dnl
dnl Written for Xfce by Benedikt Meurer <benny@xfce.org>.
dnl

dnl Version information
XDT_VERSION_INIT([4.20.2], [git])
m4_define([copyright_year], [2025])

dnl minimum required versions
m4_define([glib_min_version], [2.72.0])
m4_define([gtk_min_version], [3.24.0])
m4_define([xfce4_min_version], [4.18.0])
m4_define([libxfce4util_min_version], [4.19.2])
m4_define([libxfce4ui_min_version], [4.18.4])
m4_define([libxfce4windowing_min_version], [4.19.2])

m4_define([libx11_min_version], [1.6.7])
m4_define([libwnck_min_version], [3.10.0])
m4_define([libice_min_version], [1.0.10])
m4_define([libsm_min_version], [1.2.3])

m4_define([gtk_layer_shell_min_version], [0.7.0])

m4_define([polkit_min_version], [0.102])

dnl Initialize autoconf
AC_COPYRIGHT([Copyright (c) 2003-copyright_year()
        The Xfce development team. All rights reserved.

Written for Xfce by Benedikt Meurer <benny@xfce.org>.])
AC_INIT([xfce4-session], [xdt_version], [https://gitlab.xfce.org/xfce/xfce4-session])
AC_PREREQ([2.69])
AC_REVISION([xdt_version_build])
AC_PROG_MAKE_SET()
COPYRIGHT_YEAR=copyright_year()
AC_DEFINE_UNQUOTED([COPYRIGHT_YEAR], ["$COPYRIGHT_YEAR"], [Copyright year])
AC_SUBST([COPYRIGHT_YEAR])

dnl Initialize automake
AM_INIT_AUTOMAKE([1.11 dist-bzip2 tar-ustar no-dist-gzip foreign])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])
AM_SILENT_RULES([yes])

dnl Set helper path prefix
AC_ARG_WITH([helper-path-prefix],
            [AS_HELP_STRING([--with-helper-path-prefix=PATH],
                            [Path prefix under which helper executables will be installed (default: $libdir)])],
            [HELPER_PATH_PREFIX="$withval"],
            [HELPER_PATH_PREFIX="$libdir"])
AC_SUBST([HELPER_PATH_PREFIX])

dnl check for UNIX variants
AC_USE_SYSTEM_EXTENSIONS
AM_CONDITIONAL([HAVE_OS_CYGWIN], [test x"`uname | grep \"CYGWIN\"`" != x""])
if test x"`uname | grep \"CYGWIN\"`" != x""; then
	AC_DEFINE([HAVE_OS_CYGWIN], [1], [Compiling under cygwin])
fi

dnl check for basic programs
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_MKDIR_P
m4_ifdef([AC_PROG_SED], [AC_PROG_SED], [AC_CHECK_PROG(SED, sed, sed)])

dnl check for libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

dnl check for standard header files
AC_CHECK_HEADERS([errno.h fcntl.h \
                  netdb.h signal.h stdarg.h sys/resource.h \
                  sys/socket.h sys/time.h sys/wait.h sys/utsname.h time.h \
                  unistd.h math.h sys/types.h sys/stat.h sys/sysctl.h memory.h stdlib.h stdio.h string.h])
AC_CHECK_FUNCS([getaddrinfo gethostbyname gethostname setsid sigprocmask])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC

dnl Check for required libraries
AC_CHECK_LIB([m], [rint], [libm=-lm], [libm=])
LIBM="$libm"
AC_SUBST(LIBM)

dnl Check for i18n support
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

dnl Check for required packages
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [gtk_min_version])
XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [glib_min_version])
XDT_CHECK_PACKAGE([GIO], [gio-2.0], [glib_min_version])
XDT_CHECK_PACKAGE([GIO_UNIX], [gio-unix-2.0], [glib_min_version])
XDT_CHECK_PACKAGE([GMODULE], [gmodule-2.0], [glib_min_version])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [libxfce4util_min_version])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [libxfce4ui_min_version])
XDT_CHECK_PACKAGE([XFCONF], [libxfconf-0], [xfce4_min_version])
XDT_CHECK_PACKAGE([LIBXFCE4WINDOWING], [libxfce4windowing-0], [libxfce4windowing_min_version])

XDT_CHECK_PACKAGE_BINARY([GLIB_COMPILE_RESOURCES], [gio-2.0], [glib_compile_resources], [glib-compile-resources])

XDT_CHECK_OPTIONAL_FEATURE([X11],
                           [x11],
                           [
                             XDT_FEATURE_DEPENDENCY([GDK_X11], [gdk-x11-3.0], [gtk_min_version])
                             XDT_FEATURE_DEPENDENCY([LIBX11], [x11], [libx11_min_version])
                             XDT_FEATURE_DEPENDENCY([LIBWNCK], [libwnck-3.0], [libwnck_min_version])
                             XDT_FEATURE_DEPENDENCY([LIBICE], [ice], [libice_min_version])
                             XDT_FEATURE_DEPENDENCY([LIBSM], [sm], [libsm_min_version])
                           ],
                           [the X11 windowing system])
XDT_CHECK_OPTIONAL_FEATURE([WAYLAND],
                           [wayland],
                           [
                             XDT_FEATURE_DEPENDENCY([GDK_WAYLAND], [gdk-wayland-3.0], [gtk_min_version])
                           ],
                           [the Wayland windowing system])
if test x"$ENABLE_X11" != x"yes" -a x"$ENABLE_WAYLAND" != x"yes"; then
  AC_MSG_ERROR([Either both X11 and Wayland support was disabled, or required dependencies are missing. One of the two must be enabled.])
fi

if test x"$ENABLE_X11" = x"yes"; then
  dnl Check for _IceTransNoListen
  ac_LIBS="$LIBS"
  LIBS="$LIBS $LIBICE_LIBS"
  AC_CHECK_FUNCS([_IceTransNoListen])
  LIBS="$ac_LIBS"

  dnl Check for iceauth
  AC_PATH_PROG([ICEAUTH], [iceauth])
  if test x"$ICEAUTH" != x""; then
    AC_DEFINE_UNQUOTED([ICEAUTH_CMD], ["$ICEAUTH"], [path to iceauth])
  else
    AC_MSG_ERROR([iceauth missing, please check your X11 installation])
  fi

  dnl Find a location for the session desktop file
  AC_MSG_CHECKING([what xsession-prefix to use])
  AC_ARG_WITH([xsession-prefix],
    AS_HELP_STRING([--with-xsession-prefix=XSESSION_PREFIX], [What should be the prefix for the xsession .desktop file?]),
    [], [with_xsession_prefix="/usr"])
  XSESSION_PREFIX=$with_xsession_prefix
  AC_SUBST([XSESSION_PREFIX])
  AC_MSG_RESULT([$with_xsession_prefix])

  dnl Check for legacy session management
  AC_ARG_ENABLE([legacy-sm],
    AS_HELP_STRING([--enable-legacy-sm], [Enable X11R5 session management])
    AS_HELP_STRING([--disable-legacy-sm], [Disable X11R5 session management]),
    [], [enable_legacy_sm=yes])
  AC_MSG_CHECKING([whether to enable X11R5 session management])
  if test x"$enable_legacy_sm" != x"yes"; then
    enable_legacy_sm=no
    AC_MSG_RESULT([no])
  else
    AC_DEFINE([ENABLE_LEGACY_SESSION_MANAGEMENT], [1], [Define for X11R5 sm compat])
    AC_MSG_RESULT([yes])
  fi
fi
AM_CONDITIONAL([ENABLE_LEGACY_SESSION_MANAGEMENT], [test x"$enable_legacy_sm" = x"yes"])

if test x"$ENABLE_WAYLAND" = x"yes"; then
  dnl Find a location for the session desktop file
  AC_MSG_CHECKING([what wayland-session-prefix to use])
  AC_ARG_WITH([wayland-session-prefix],
    AS_HELP_STRING([--with-wayland-session-prefix=WAYLAND_SESSION_PREFIX], [What should be the prefix for the wayland session .desktop file?]),
    [], [with_wayland_session_prefix="/usr"])
  WAYLAND_SESSION_PREFIX=$with_wayland_session_prefix
  AC_SUBST([WAYLAND_SESSION_PREFIX])
  AC_MSG_RESULT([$with_wayland_session_prefix])
else
  dnl Do not check for optional Wayland packages if Wayland is disabled
  enable_gtk_layer_shell=no
fi
XDT_CHECK_OPTIONAL_PACKAGE([GTK_LAYER_SHELL], [gtk-layer-shell-0], [gtk_layer_shell_min_version], [gtk-layer-shell], [Gtk Layer Shell support])

XDT_CHECK_PACKAGE_BINARY([GDBUS_CODEGEN], [gio-2.0], [gdbus_codegen], [gdbus-codegen])

dnl Check for Polkit/PolicyKit
XDT_CHECK_OPTIONAL_PACKAGE([POLKIT], [polkit-gobject-1], [polkit_min_version],
                           [polkit], [Polkit support])

dnl Check for debugging support
XDT_FEATURE_DEBUG([xdt_debug_default])

dnl Check for linker optimizations
XDT_FEATURE_LINKER_OPTS()

dnl Compile time default choice of backend
AC_ARG_WITH([backend],
	    AS_HELP_STRING([--with-backend=<option>],
			   [Default backend to use linux, freebsd, openbsd, solaris]))
# default to a sane option
AC_CANONICAL_HOST
if test x$with_backend = x; then
	AS_CASE([$host],
		[*-linux*],   [with_backend=linux],
		[*-*freebsd*], [with_backend=freebsd],
		[*-*dragonfly*], [with_backend=freebsd],
		[*-openbsd*], [with_backend=openbsd],
		[*-solaris*], [with_backend=solaris])
fi
AC_DEFINE_UNQUOTED(BACKEND, "$with_backend", [backend])
AC_SUBST(BACKEND, "$with_backend")

if test x$with_backend = xlinux; then
    AC_DEFINE(BACKEND_TYPE_LINUX, 1, [Linux suspend/hibernate/hybrid-sleep backend])
fi
if test x$with_backend = xfreebsd; then
    AC_DEFINE(BACKEND_TYPE_FREEBSD, 1, [FreeBSD suspend/hibernate backend])
fi
if test x$with_backend = xopenbsd; then
    AC_DEFINE(BACKEND_TYPE_OPENBSD, 1, [OpenBSD suspend/hibernate backend])
fi
if test x$with_backend = xsolaris; then
    AC_DEFINE(BACKEND_TYPE_SOLARIS, 1, [Solaris suspend/hibernate backend])
fi

AC_CONFIG_FILES([
Makefile
doc/Makefile
icons/Makefile
icons/16x16/Makefile
icons/24x24/Makefile
icons/24x24-actions/Makefile
icons/32x32/Makefile
icons/48x48-actions/Makefile
icons/48x48/Makefile
icons/64x64/Makefile
icons/96x96/Makefile
icons/96x96-actions/Makefile
icons/128x128/Makefile
icons/scalable/Makefile
labwc/Makefile
libxfsm/Makefile
po/Makefile.in
settings/Makefile
scripts/Makefile
xfce4-session/Makefile
xfce4-session-logout/Makefile
xfsm-shutdown-helper/Makefile
])
AC_OUTPUT

dnl ***************************
dnl *** Print configuration ***
dnl ***************************
echo
echo "Build Configuration:"
echo
echo " * Debugging support:              $enable_debug"
echo " * X11 Support:                    ${ENABLE_X11:-no}"
if test x"$ENABLE_X11" = x"yes"; then
echo "   * Legacy session management:    $enable_legacy_sm"
fi
echo " * Wayland Support:                ${ENABLE_WAYLAND:-no}"
if test x"$ENABLE_WAYLAND" = x"yes"; then
echo "   * Gtk Layer Shell support:      ${GTK_LAYER_SHELL_FOUND:-no}"
fi
echo " * Polkit/PolicyKit support:       ${POLKIT_FOUND:-no}"
echo " * Backend:                        ${with_backend}"

echo
