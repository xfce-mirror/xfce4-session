man_MANS =								\
	xfce4-session-logout.1

bin_PROGRAMS = 								\
	xfce4-session-logout

xfce4_session_logout_SOURCES =						\
	main.c

xfce4_session_logout_CFLAGS =						\
	-I$(top_srcdir)							\
	$(LIBXFCEGUI4_CFLAGS)

xfce4_session_logout_LDFLAGS =						\
	$(LIBXFCEGUI4_LIBS)

EXTRA_DIST =								\
	$(man_MANS)

