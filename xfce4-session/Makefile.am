bin_PROGRAMS = xfce4-session

xfce4_session_SOURCES =							\
	client.c							\
	client.h							\
	client-list.c							\
	client-list.h							\
	ice-layer.c							\
	ice-layer.h							\
	main.c								\
	manager.c							\
	manager.h							\
	shutdown.c							\
	shutdown.h							\
	splash-fallback.h						\
	splash-screen.c							\
	splash-screen.h							\
	startup.c							\
	startup.h							\
	tray-icon.c							\
	tray-icon.h							\
	util.c								\
	util.h

xfce4_session_CFLAGS =							\
	@LIBSM_CFLAGS@							\
	@LIBX11_CFLAGS@							\
	@LIBXFCE4MCS_CLIENT_CFLAGS@					\
	@LIBXFCEGUI4_CFLAGS@						\
	-DDEFAULT_SESSION=\"$(sysconfdir)/xfce4/default.session\"	\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DSHUTDOWN_COMMAND=\"$(sbindir)/xfce4-shutdown\"		\
	-DSPLASH_THEMES_DIR=\"$(datadir)/xfce4/splash\"

xfce4_session_LDADD =							\
	@LIBSM_LDFLAGS@							\
	@LIBSM_LIBS@							\
	@LIBX11_LDFLAGS@						\
	@LIBX11_LIBS@							\
	@LIBXFCE4MCS_CLIENT_LIBS@					\
	@LIBXFCEGUI4_LIBS@

EXTRA_DIST =								\
	splash-fallback.png

splash-fallback.h: $(srcdir)/splash-fallback.png
	gdk-pixbuf-csource --raw --build-list				\
	fallback_logo $(srcdir)/splash-fallback.png > splash-fallback.h
