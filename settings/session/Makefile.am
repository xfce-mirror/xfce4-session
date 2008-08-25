bin_PROGRAMS = xfce4-session-settings
	

xfce4_session_settings_SOURCES =						\
	session.c

xfce4_session_settings_CFLAGS =						\
	-I$(top_srcdir)							\
	$(LIBXFCEGUI4_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	-DLOCALEDIR=\"$(localedir)\"

xfce4_session_settings_LDADD =						\
	$(top_builddir)/libxfsm/libxfsm-4.2.la				\
	$(LIBXFCEGUI4_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)						\
	$(DBUS_LIBS)	


desktopdir = $(datadir)/applications
desktop_in_files = xfce-session-settings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	$(desktop_in_files)

DISTCLEANFILES =							\
	$(desktop_DATA)

