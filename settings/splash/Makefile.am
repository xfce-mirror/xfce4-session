bin_PROGRAMS = xfce4-splash-settings
	

xfce4_splash_settings_SOURCES =						\
	module.c							\
	module.h							\
	nopreview.h							\
	splash.c

xfce4_splash_settings_CFLAGS =						\
	-I$(top_srcdir)							\
	$(LIBXFCEGUI4_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	-DMODULESDIR=\"$(libdir)/xfce4/splash/engines\"			\
	-DLOCALEDIR=\"$(localedir)\"

xfce4_splash_settings_LDADD =						\
	$(top_builddir)/libxfsm/libxfsm-4.2.la \
	$(LIBXFCEGUI4_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)						\
	$(DBUS_LIBS)	

xfce4_splash_settings_DEPENDENCIES =					\
	$(top_builddir)/libxfsm/libxfsm-4.2.la

nopreview.h: $(srcdir)/nopreview.png
	gdk-pixbuf-csource --struct --name=nopreview			\
		$(srcdir)/nopreview.png > nopreview.h

desktopdir = $(datadir)/applications
desktop_in_files = xfce-splash-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	nopreview.h							\
	nopreview.png							\
	$(desktop_in_files)

DISTCLEANFILES =							\
	$(desktop_DATA)

