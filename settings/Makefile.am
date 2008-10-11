bin_PROGRAMS = xfce4-session-settings

xfce4_session_settings_SOURCES = \
	main.c \
	module.c \
	module.h \
	nopreview.h \
	session-editor.c \
	splash-settings.c  \
	startup-settings.c \
	xfae-dialog.c \
	xfae-dialog.h \
	xfae-model.c \
	xfae-model.h \
	xfae-window.c \
	xfae-window.h \
	xfce4-session-settings-common.h \
	xfce4-session-settings_glade.h

xfce4_session_settings_CFLAGS = \
	-DLOCALEDIR=\"$(localedir)\" \
	-DMODULESDIR=\"$(libdir)/xfce4/splash/engines\" \
	-DG_LOG_DOMAIN=\"xfce4-session-settings\" \
	-I$(top_srcdir) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCEGUI4_CFLAGS) \
	$(LIBGLADE_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(DBUS_GLIB_CFLAGS)

xfce4_session_settings_LDADD = \
	$(top_builddir)/libxfsm/libxfsm-4.6.la \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCEGUI4_LIBS) \
	$(LIBGLADE_LIBS) \
	$(XFCONF_LIBS) \
	$(DBUS_GLIB_LIBS)

xfce4_session_settings_DEPENDENCIES = \
	$(top_builddir)/libxfsm/libxfsm-4.6.la

settingsdir = $(sysconfdir)/xdg/xfce4/xfconf/xfce-perchannel-xml
settings_DATA = xfce4-session.xml

desktopdir = $(datadir)/applications
desktop_in_files = xfce-session-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@


if MAINTAINER_MODE

BUILT_SOURCES = \
	nopreview.h \
	xfce4-session-settings_glade.h

nopreview.h: $(srcdir)/nopreview.png
	gdk-pixbuf-csource --struct --name=nopreview $< > $@

xfce4-session-settings_glade.h: xfce4-session-settings.glade
	exo-csource --static --strip-comments --strip-content --name=xfce4_session_settings_glade $< >$@

endif


EXTRA_DIST = \
	$(desktop_in_files) \
	nopreview.png \
	xfce4-session-settings.glade

DISTCLEANFILES = \
	$(desktop_DATA)
