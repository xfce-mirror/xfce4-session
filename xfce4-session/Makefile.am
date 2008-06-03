# $Id$

INCLUDES =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)

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
	xfsm-chooser.c							\
	xfsm-chooser.h							\
	xfsm-client.c							\
	xfsm-client.h							\
	xfsm-compat-gnome.c						\
	xfsm-compat-gnome.h						\
	xfsm-compat-kde.c						\
	xfsm-compat-kde.h						\
	xfsm-dns.c							\
	xfsm-dns.h							\
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
	xfsm-startup.c							\
	xfsm-startup.h

xfce4_session_CFLAGS =							\
	$(GNOME_CFLAGS)							\
	$(LIBSM_CFLAGS)							\
	$(LIBX11_CFLAGS)						\
	$(LIBXFCEGUI4_CFLAGS)						\
	$(DBUS_CFLAGS)							\
	$(LIBWNCK_CFLAGS)						\
	-DDBUS_API_SUBJECT_TO_CHANGE					\
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE					\
	-DLIBDIR=\"$(libdir)\"						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DSYSCONFDIR=\"$(sysconfdir)\"					\
	-DXFSM_SHUTDOWN_HELPER=\"$(libexecdir)/xfsm-shutdown-helper\"

xfce4_session_LDADD =							\
	$(top_builddir)/libxfsm/libxfsm-4.2.la				\
	$(LIBSM_LDFLAGS)						\
	$(LIBSM_LIBS)							\
	$(LIBX11_LDFLAGS)						\
	$(LIBX11_LIBS)							\
	$(LIBXFCEGUI4_LIBS)						\
	$(DBUS_LIBS)							\
	$(LIBWNCK_LIBS)							\
	$(GNOME_LIBS)

xfce4_session_DEPENDENCIES =						\
	$(top_builddir)/libxfsm/libxfsm-4.2.la


chooser-icon.h: $(srcdir)/chooser-icon.png
	gdk-pixbuf-csource --struct --name=chooser_icon_data		\
		$(srcdir)/chooser-icon.png > chooser-icon.h

EXTRA_DIST =								\
	$(man_MANS)							\
	chooser-icon.png

