splashdir = $(libdir)/xfce4/mcs-plugins

splash_LTLIBRARIES = 							\
	splash_settings.la

splash_settings_la_LDFLAGS =						\
	-avoid-version							\
	-module								\
	-no-undefined

splash_settings_la_SOURCES =						\
	module.c							\
	module.h							\
	nopreview.h							\
	splash.c

splash_settings_la_CFLAGS =						\
	-I$(top_srcdir)							\
	@XFCE_MCS_MANAGER_CFLAGS@					\
	-DMODULESDIR=\"$(libdir)/xfce4/splash/engines\"			\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

splash_settings_la_LIBADD =						\
	$(top_builddir)/libxfsm/libxfsm-4.2.la

if HAVE_OS_CYGWIN
splash_settings_la_LDFLAGS +=						\
	-export-symbols $(datadir)/xfce4/devel/mcs-manager.def

splash_settings_la_LIBADD +=						\
	@XFCE_MCS_MANAGER_LIBS@						\
	@LIBX11_LIBS@
endif

splash_settings_la_DEPENDENCIES =					\
	$(top_builddir)/libxfsm/libxfsm-4.2.la


EXTRA_DIST =								\
	nopreview.h							\
	nopreview.png							\
	xfce-splash-settings.desktop

nopreview.h: $(srcdir)/nopreview.png
	gdk-pixbuf-csource --struct --name=nopreview			\
		$(srcdir)/nopreview.png > nopreview.h

appsdir = $(datadir)/applications
apps_DATA = xfce-splash-settings.desktop
