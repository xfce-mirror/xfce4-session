
INCLUDES = \
	-I$(top_srcdir) \
	-DLOCALEDIR=\"$(localedir)\" \
	-DG_LOG_DOMAIN=\"libxfsm-logout-plugin\"

plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = libxfsm-logout-plugin.la

libxfsm_logout_plugin_built_sources = \
	xfsm-logout-plugin-ui.h

libxfsm_logout_plugin_la_SOURCES = \
	$(libxfsm_logout_plugin_built_sources) \
	xfsm-logout-plugin.c

libxfsm_logout_plugin_la_CFLAGS = \
	$(LIBX11_CFLAGS) \
	$(LIBXFCE4PANEL_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(GTK_CFLAGS) \
	$(DBUS_GLIB_CFLAGS)

libxfsm_logout_plugin_la_LIBADD = \
	$(LIBX11_LIBS) \
	$(LIBXFCE4PANEL_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(GTK_LIBS) \
	$(DBUS_GLIB_LIBS)

libxfsm_logout_plugin_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)'

#
# .desktop file
#
desktopdir = $(datadir)/xfce4/panel-plugins
desktop_in_files = xfsm-logout-plugin.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	xfsm-logout-plugin-ui.xml \
	$(desktop_in_files)

DISTCLEANFILES = \
	$(desktop_DATA)

if MAINTAINER_MODE
BUILT_SOURCES = \
	$(libxfsm_logout_plugin_built_sources)

DISTCLEANFILES += \
	$(libxfsm_logout_plugin_built_sources)

xfsm-logout-plugin-ui.h: $(srcdir)/xfsm-logout-plugin-ui.xml Makefile
	$(AM_V_GEN) exo-csource --static --strip-comments --strip-content --name=logout_plugin_ui $< > $@
endif

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
