splashdir = @XFCE_MCS_MANAGER_PLUGINSDIR@

splash_LTLIBRARIES = 							\
	splash_settings.la

splash_settings_la_LDFLAGS =						\
	-avoid-version							\
	-module

splash_settings_la_SOURCES =						\
	module.c							\
	module.h							\
	splash.c

splash_settings_la_CFLAGS =						\
	@XFCE_MCS_MANAGER_CFLAGS@					\
	-DMODULESDIR=\"$(libdir)/xfce4/splash/engines\"			\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

splash_settings_la_LIBADD =						\
	$(top_builddir)/xfce4-session/libxfsm.la

splash_settings_la_DEPENDENCIES =					\
	$(top_builddir)/xfce4-session/libxfsm.la


EXTRA_DIST =								\
	xfce-splash-settings.desktop

appsdir = $(datadir)/applications
apps_DATA = xfce-splash-settings.desktop
