plugin_sources = \
	xfsm-logout-plugin.c \
	xfsm-logout-plugin-ui.h

plugin_cflags = \
	$(LIBX11_CFLAGS) \
	$(LIBXFCE4PANEL_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(GTK_CFLAGS) \
	$(DBUS_GLIB_CFLAGS) \
	-DLOCALEDIR=\"$(localedir)\"

plugin_libs = \
	$(LIBX11_LIBS) \
	$(LIBXFCE4PANEL_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(GTK_LIBS) \
	$(DBUS_GLIB_LIBS)

if BUILD_OLD_PANEL_PLUGIN

plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfsm-logout-plugin

xfsm_logout_plugin_SOURCES = $(plugin_sources)
xfsm_logout_plugin_CFLAGS = $(plugin_cflags)
xfsm_logout_plugin_LDADD = $(plugin_libs)

else

plugindir = $(libdir)/xfce4/panel-plugins
plugin_LTLIBRARIES = libxfsm-logout-plugin.la

libxfsm_logout_plugin_la_SOURCES = $(plugin_sources)
libxfsm_logout_plugin_la_CFLAGS = $(plugin_cflags)
libxfsm_logout_plugin_la_LIBADD = $(plugin_libs)
libxfsm_logout_plugin_la_LDFLAGS = \
	-avoid-version \
	-export-dynamic \
	-module \
	-shared

endif

# .desktop file
#
# Some automake trickery here. Because we cannot use $(libexecdir) in the
# automake stage, we'll use sed to get the full path into the .desktop file.
# We also need to let intltool merge the translated fields, so we add an
# additional level of indirection: a <name>.desktop.in.in file.
#
if BUILD_OLD_PANEL_PLUGIN
dfile = xfsm-logout-plugin-4.6.desktop.in.in
else
dfile = xfsm-logout-plugin.desktop.in.in
endif

desktop_in_in_files = $(dfile)
desktop_in_files = xfsm-logout-plugin.desktop.in

desktopdir = $(datadir)/xfce4/panel-plugins
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

# get full path into .desktop file
xfsm-logout-plugin.desktop.in: $(srcdir)/$(dfile)
	sed -e "s,\@plugindir\@,$(plugindir)," $< > $@

if MAINTAINER_MODE

xfsm-logout-plugin-ui.h: $(srcdir)/xfsm-logout-plugin-ui.xml Makefile
	exo-csource --static --name=logout_plugin_ui $< > $@

BUILT_SOURCES = xfsm-logout-plugin-ui.h

MAINTAINERCLEANFILES = $(BUILT_SOURCES)

endif

EXTRA_DIST = \
	xfsm-logout-plugin.desktop.in.in \
	xfsm-logout-plugin-4.6.desktop.in.in \
	xfsm-logout-plugin-ui.xml

DISTCLEANFILES = \
	$(desktop_DATA) \
	$(desktop_in_files)
