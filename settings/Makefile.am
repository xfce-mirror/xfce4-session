sessiondir = @XFCE_MCS_MANAGER_PLUGINSDIR@

session_LTLIBRARIES = 							\
	session_settings.la

session_settings_la_LDFLAGS =						\
	-avoid-version							\
	-module

session_settings_la_SOURCES =						\
	session-icon.h							\
	session.c

session_settings_la_CFLAGS =						\
	@XFCE_MCS_MANAGER_CFLAGS@					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

session-icon.h: $(srcdir)/session.png
	gdk-pixbuf-csource --raw --build-list				\
	session_icon_data $(srcdir)/session.png > session-icon.h

noinst_DATA =								\
	session-icon.h

EXTRA_DIST =								\
	session.png								\
	xfce-session-settings.desktop

appsdir = $(datadir)/applications
apps_DATA = xfce-session-settings.desktop
