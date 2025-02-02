bin_PROGRAMS = xfce4-session-settings

xfce4_session_settings_SOURCES = \
	main.c \
	session-editor.c \
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
	xfsm-client-dbus-client.c \
	xfsm-manager-dbus-client.h \
	xfsm-manager-dbus-client.c

xfce4_session_settings_CFLAGS = \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"xfce4-session-settings\" \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libxfsm \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GMODULE_CFLAGS)

xfce4_session_settings_LDADD = \
	$(top_builddir)/libxfsm/libxfsm.la \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_LIBS) \
	$(XFCONF_LIBS) \
	$(GIO_LIBS) \
	$(GMODULE_LIBS)

settingsdir = $(sysconfdir)/xdg/xfce4/xfconf/xfce-perchannel-xml
settings_DATA = xfce4-session.xml

desktopdir = $(datadir)/applications
desktop_in_files = xfce-session-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@


BUILT_SOURCES = \
	xfce4-session-marshal.c \
	xfce4-session-marshal.h \
	xfce4-session-settings_ui.h \
	xfsm-client-dbus-client.h \
	xfsm-client-dbus-client.c \
	xfsm-manager-dbus-client.h \
	xfsm-manager-dbus-client.c

xfce4-session-marshal.h: stamp-xfce4-session-marshal.h
	@true
stamp-xfce4-session-marshal.h: $(top_srcdir)/settings/xfce4-session-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=xfce4_session_marshal $(top_srcdir)/settings/xfce4-session-marshal.list --header > xfce4-session-marshal.h
	echo timestamp > $(@F)
xfce4-session-marshal.c: $(top_srcdir)/settings/xfce4-session-marshal.list Makefile
	$(AM_V_GEN) echo '#include "xfce4-session-marshal.h"' > $@
	glib-genmarshal --prefix=xfce4_session_marshal $(top_srcdir)/settings/xfce4-session-marshal.list --body >> xfce4-session-marshal.c

xfce4-session-settings_ui.h: xfce4-session-settings.ui
	$(AM_V_GEN) xdt-csource --static --strip-comments --strip-content --name=xfce4_session_settings_ui $< >$@

xfsm-client-dbus-client.c xfsm-client-dbus-client.h : $(top_srcdir)/xfce4-session/xfsm-client-dbus.xml Makefile.am
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--c-namespace=Xfsm \
		--interface-prefix=org.xfce.Session. \
		--generate-c-code=xfsm-client-dbus-client \
		$(top_srcdir)/xfce4-session/xfsm-client-dbus.xml

xfsm-manager-dbus-client.c xfsm-manager-dbus-client.h : $(top_srcdir)/xfce4-session/xfsm-manager-dbus.xml Makefile.am
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--c-namespace=Xfsm \
		--interface-prefix=org.xfce.Session. \
		--generate-c-code=xfsm-manager-dbus-client \
		$(top_srcdir)/xfce4-session/xfsm-manager-dbus.xml


EXTRA_DIST = \
	$(desktop_in_files) \
	xfce4-session.xml \
	xfce4-session-settings.ui \
	xfce4-session-marshal.list \
	$(top_srcdir)/xfce4-session/xfsm-client-dbus.xml \
	$(top_srcdir)/xfce4-session/xfsm-manager-dbus.xml \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_DATA) \
	$(BUILT_SOURCES) \
	stamp-xfce4-session-marshal.h \
	$(NULL)
