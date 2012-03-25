bin_PROGRAMS = xfce4-session-settings

xfce4_session_settings_SOURCES = \
	main.c \
	module.c \
	module.h \
	session-editor.c \
	splash-settings.c \
	xfae-dialog.c \
	xfae-dialog.h \
	xfae-model.c \
	xfae-model.h \
	xfae-window.c \
	xfae-window.h \
	xfce4-session-marshal.c \
	xfce4-session-marshal.h \
	xfce4-session-settings-common.h \
	xfce4-session-settings_ui.h \
	xfsm-client-dbus-client.h \
	xfsm-manager-dbus-client.h

xfce4_session_settings_CFLAGS = \
	-DLOCALEDIR=\"$(localedir)\" \
	-DMODULESDIR=\"$(libdir)/xfce4/session/splash-engines\" \
	-DG_LOG_DOMAIN=\"xfce4-session-settings\" \
	-I$(top_srcdir) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	$(GMODULE_CFLAGS)

xfce4_session_settings_LDADD = \
	$(top_builddir)/libxfsm/libxfsm-4.6.la \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_LIBS) \
	$(XFCONF_LIBS) \
	$(DBUS_GLIB_LIBS) \
	$(GMODULE_LIBS)

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
	xfce4-session-marshal.c \
	xfce4-session-marshal.h \
	xfce4-session-settings_ui.h \
	xfsm-client-dbus-client.h \
	xfsm-manager-dbus-client.h

xfce4-session-marshal.h: stamp-xfce4-session-marshal.h
	@true
stamp-xfce4-session-marshal.h: xfce4-session-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=xfce4_session_marshal xfce4-session-marshal.list --header > xfce4-session-marshal.h
	echo timestamp > $(@F)
xfce4-session-marshal.c: xfce4-session-marshal.list Makefile
	$(AM_V_GEN) echo '#include "xfce4-session-marshal.h"' > $@
	glib-genmarshal --prefix=xfce4_session_marshal xfce4-session-marshal.list --body >> xfce4-session-marshal.c

xfce4-session-settings_ui.h: xfce4-session-settings.glade
	$(AM_V_GEN) exo-csource --static --strip-comments --strip-content --name=xfce4_session_settings_ui $< >$@

xfsm-client-dbus-client.h: $(top_srcdir)/xfce4-session/xfsm-client-dbus.xml Makefile
	$(AM_V_GEN) dbus-binding-tool --mode=glib-client $(top_srcdir)/xfce4-session/xfsm-client-dbus.xml > $@

xfsm-manager-dbus-client.h: $(top_srcdir)/xfce4-session/xfsm-manager-dbus.xml Makefile
	$(AM_V_GEN) dbus-binding-tool --mode=glib-client $(top_srcdir)/xfce4-session/xfsm-manager-dbus.xml > $@

endif


EXTRA_DIST = \
	$(desktop_in_files) \
	xfce4-session.xml \
	xfce4-session-settings.glade

DISTCLEANFILES = \
	$(desktop_DATA)
