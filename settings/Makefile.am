sessiondir = @XFCE_MCS_MANAGER_PLUGINSDIR@

session_LTLIBRARIES = 							\
	session_settings.la

session_settings_la_LDFLAGS =						\
	-avoid-version							\
	-module

session_settings_la_SOURCES =						\
	gnome-uri.c							\
	gnome-uri.h							\
	looknfeel.c							\
	looknfeel.h							\
	session-icon.h							\
	session.c							\
	../xfce4-session/xfsm-splash-theme.c

session_settings_la_CFLAGS =						\
	@XFCE_MCS_MANAGER_CFLAGS@					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DXFSM_INSTALL_THEME=\"$(libexecdir)/xfsm-install-theme\"

session-icon.h: $(srcdir)/session.png
	gdk-pixbuf-csource --raw --build-list				\
	session_icon_data $(srcdir)/session.png > session-icon.h

libexec_SCRIPTS = xfsm-install-theme

noinst_DATA =								\
	session-icon.h

EXTRA_DIST =								\
	session.png								\
	xfce-session-settings.desktop

appsdir = $(datadir)/applications
apps_DATA = xfce-session-settings.desktop
