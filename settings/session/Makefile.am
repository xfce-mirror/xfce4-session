sessiondir = @XFCE_MCS_MANAGER_PLUGINSDIR@

session_LTLIBRARIES = 							\
	session_settings.la

session_settings_la_LDFLAGS =						\
	-avoid-version							\
	-no-undefined							\
	-module

session_settings_la_SOURCES =						\
	session.c

session_settings_la_CFLAGS =						\
	-I$(top_srcdir)							\
	@XFCE_MCS_MANAGER_CFLAGS@					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

if HAVE_OS_CYGWIN
session_settings_la_LDFLAGS +=						\
	-export-symbols $(datadir)/xfce4/devel/mcs-manager.def

session_settings_la_LIBADD =						\
	@XFCE_MCS_MANAGER_LIBS@
endif

EXTRA_DIST =								\
	xfce-session-settings.desktop

appsdir = $(datadir)/applications
apps_DATA = xfce-session-settings.desktop
