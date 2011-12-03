man_MANS =								\
	xfce4-session-logout.1

bin_PROGRAMS = 								\
	xfce4-session-logout

xfce4_session_logout_SOURCES =						\
	main.c

xfce4_session_logout_CFLAGS =						\
	-I$(top_srcdir)							\
	$(DBUS_CFLAGS)							\
	$(DBUS_GLIB_CFLAGS)						\
	$(LIBXFCE4UI_CFLAGS)						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

xfce4_session_logout_LDFLAGS =						\
	$(DBUS_LIBS)							\
	$(DBUS_GLIB_LIBS)						\
	$(LIBXFCE4UI_LIBS)

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-session-logout.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	$(man_MANS)							\
	$(desktop_in_files)

DISTCLEANFILES = 							\
	$(desktop_DATA)
