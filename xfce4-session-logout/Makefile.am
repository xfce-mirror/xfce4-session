man_MANS =								\
	xfce4-session-logout.1

bin_PROGRAMS = 								\
	xfce4-session-logout

xfce4_session_logout_SOURCES =						\
	main.c

xfce4_session_logout_CFLAGS =						\
	-I$(top_srcdir)							\
	$(GIO_CFLAGS)							\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(GTK_CFLAGS) \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

xfce4_session_logout_LDFLAGS =						\
	$(GIO_LIBS)							\
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(GTK_LIBS) \
	$(NULL)

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-session-logout.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST =								\
	$(man_MANS)							\
	$(desktop_in_files) \
	meson.build \
	$(NULL)

DISTCLEANFILES = 							\
	$(desktop_DATA)
