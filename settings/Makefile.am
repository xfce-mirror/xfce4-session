SUBDIRS = scripts

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
	session.c							\
	../xfce4-session/xfsm-splash-theme.c

session_settings_la_CFLAGS =						\
	@XFCE_MCS_MANAGER_CFLAGS@					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DXFSM_EXPORT_THEME=\"$(libexecdir)/xfsm-export-theme\"		\
	-DXFSM_INSTALL_THEME=\"$(libexecdir)/xfsm-install-theme\"

EXTRA_DIST =								\
	xfce-session-settings.desktop

appsdir = $(datadir)/applications
apps_DATA = xfce-session-settings.desktop
