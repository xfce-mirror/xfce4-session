man_MANS = xfce4-session.1

bin_PROGRAMS = xfce4-session

xfce4_session_SOURCES =							\
	ice-layer.c							\
	ice-layer.h							\
	main.c								\
	shutdown.c							\
	shutdown.h							\
	sm-layer.c							\
	sm-layer.h							\
	xfsm-chooser.c							\
	xfsm-chooser.h							\
	xfsm-client.c							\
	xfsm-client.h							\
	xfsm-global.c							\
	xfsm-global.h							\
	xfsm-manager.c							\
	xfsm-manager.h							\
	xfsm-properties.c						\
	xfsm-properties.h						\
	xfsm-splash-screen.c						\
	xfsm-splash-screen.h						\
	xfsm-startup.c							\
	xfsm-startup.h							\
	xfsm-util.h							\
	xfsm-util.c

xfce4_session_CFLAGS =							\
	@LIBSM_CFLAGS@							\
	@LIBX11_CFLAGS@							\
	@LIBXFCE4MCS_CLIENT_CFLAGS@					\
	@LIBXFCEGUI4_CFLAGS@						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DSHUTDOWN_COMMAND=\"$(sbindir)/xfce4-shutdown\"		\
	-DSPLASH_THEMES_DIR=\"$(datadir)/xfce4/splash\"			\
	-DSPLASH_THEME_LOGO=\"$(datadir)/pixmaps/xfce4-splash.png\"

xfce4_session_LDADD =							\
	@LIBSM_LDFLAGS@							\
	@LIBSM_LIBS@							\
	@LIBX11_LDFLAGS@						\
	@LIBX11_LIBS@							\
	@LIBXFCE4MCS_CLIENT_LIBS@					\
	@LIBXFCEGUI4_LIBS@

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = xfce4-splash.png
EXTRA_DIST =								\
	$(man_MANS)							\
	$(pixmap_DATA)
