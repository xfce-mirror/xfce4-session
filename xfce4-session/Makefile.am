man_MANS = xfce4-session.1

bin_PROGRAMS = xfce4-session

xfce4_session_SOURCES =							\
	chooser-icon.h							\
	ice-layer.c							\
	ice-layer.h							\
	main.c								\
	shutdown.c							\
	shutdown.h							\
	sm-layer.c							\
	sm-layer.h							\
	xfsm-chooser-trash.c						\
	xfsm-chooser-trash.h						\
	xfsm-chooser.c							\
	xfsm-chooser.h							\
	xfsm-client.c							\
	xfsm-client.h							\
	xfsm-compat-gnome.c						\
	xfsm-compat-gnome.h						\
	xfsm-compat-kde.c						\
	xfsm-compat-kde.h						\
	xfsm-fadeout.c							\
	xfsm-fadeout.h							\
	xfsm-global.c							\
	xfsm-global.h							\
	xfsm-legacy.c							\
	xfsm-legacy.h							\
	xfsm-manager.c							\
	xfsm-manager.h							\
	xfsm-properties.c						\
	xfsm-properties.h						\
	xfsm-shutdown-helper.c						\
	xfsm-shutdown-helper.h						\
	xfsm-splash-screen.c						\
	xfsm-splash-screen.h						\
	xfsm-splash-theme.c						\
	xfsm-splash-theme.h						\
	xfsm-startup.c							\
	xfsm-startup.h							\
	xfsm-util.h							\
	xfsm-util.c

xfce4_session_CFLAGS =							\
	@GNOME_CFLAGS@							\
	@LIBSM_CFLAGS@							\
	@LIBX11_CFLAGS@							\
	@LIBXFCE4MCS_CLIENT_CFLAGS@					\
	@LIBXFCEGUI4_CFLAGS@						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DXFSM_SHUTDOWN_HELPER=\"$(libexecdir)/xfsm-shutdown-helper\"

xfce4_session_LDADD =							\
	@GNOME_LIBS@							\
	@LIBSM_LDFLAGS@							\
	@LIBSM_LIBS@							\
	@LIBX11_LDFLAGS@						\
	@LIBX11_LIBS@							\
	@LIBXFCE4MCS_CLIENT_LIBS@					\
	@LIBXFCEGUI4_LIBS@

chooser-icon.h: $(srcdir)/chooser-icon.png
	gdk-pixbuf-csource --raw --build-list                           \
	chooser_icon_data $^ > $@

EXTRA_DIST =								\
	$(man_MANS)							\
	chooser-icon.png
