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
	xfsm-client-dbus.h						\
	xfsm-compat-gnome.c						\
	xfsm-compat-gnome.h						\
	xfsm-compat-kde.c						\
	xfsm-compat-kde.h						\
	xfsm-dns.c							\
	xfsm-dns.h							\
	xfsm-error.c							\
	xfsm-error.h							\
	xfsm-fadeout.c							\
	xfsm-fadeout.h							\
	xfsm-global.c							\
	xfsm-global.h							\
	xfsm-legacy.c							\
	xfsm-legacy.h							\
	xfsm-manager.c							\
	xfsm-manager.h							\
	xfsm-manager-dbus.h						\
	xfsm-marshal.c							\
	xfsm-marshal.h							\
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
	$(GNOME_KEYRING_CFLAGS)						\
	$(LIBSM_CFLAGS)							\
	$(LIBX11_CFLAGS)						\
	$(LIBXFCE4UI_CFLAGS)						\
	$(DBUS_CFLAGS)							\
	$(DBUS_GLIB_CFLAGS)						\
	$(LIBWNCK_CFLAGS)						\
	$(XFCONF_CFLAGS)						\
	-DDBUS_API_SUBJECT_TO_CHANGE					\
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE					\
	-DLIBDIR=\"$(libdir)\"						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DSYSCONFDIR=\"$(sysconfdir)\"					\
	-DXFSM_SHUTDOWN_HELPER_CMD=\"$(libexecdir)/xfsm-shutdown-helper\"

xfce4_session_LDADD =							\
	$(top_builddir)/libxfsm/libxfsm-4.6.la				\
	$(LIBSM_LDFLAGS)						\
	$(LIBSM_LIBS)							\
	$(LIBX11_LDFLAGS)						\
	$(LIBX11_LIBS)							\
	$(LIBXFCE4UI_LIBS)						\
	$(DBUS_LIBS)							\
	$(DBUS_GLIB_LIBS)						\
	$(LIBWNCK_LIBS)							\
	$(XFCONF_LIBS)							\
	$(GNOME_LIBS)							\
	$(GNOME_KEYRING_LIBS)

xfce4_session_DEPENDENCIES =						\
	$(top_builddir)/libxfsm/libxfsm-4.6.la


if MAINTAINER_MODE

BUILT_SOURCES =								\
	xfsm-client-dbus.h						\
	xfsm-manager-dbus.h						\
	xfsm-marshal.c							\
	xfsm-marshal.h							\
	chooser-icon.h

chooser-icon.h: $(srcdir)/chooser-icon.png
	$(AM_V_GEN) gdk-pixbuf-csource --static --raw --stream --name=chooser_icon_data $< > $@

xfsm-marshal.h: stamp-xfsm-marshal.h
	@true
stamp-xfsm-marshal.h: xfsm-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=xfsm_marshal xfsm-marshal.list --header > xfsm-marshal.h
	echo timestamp > $(@F)
xfsm-marshal.c: xfsm-marshal.list Makefile
	$(AM_V_GEN) echo '#include "xfsm-marshal.h"' > $@
	glib-genmarshal --prefix=xfsm_marshal xfsm-marshal.list --body >> xfsm-marshal.c

xfsm-manager-dbus.h: $(srcdir)/xfsm-manager-dbus.xml
	$(AM_V_GEN) dbus-binding-tool --mode=glib-server --prefix=xfsm_manager $< > $@

xfsm-client-dbus.h: $(srcdir)/xfsm-client-dbus.xml
	$(AM_V_GEN) dbus-binding-tool --mode=glib-server --prefix=xfsm_client $< > $@

DISTCLEANFILES =								\
	$(BUILT_SOURCES)

endif


EXTRA_DIST =								\
	$(man_MANS)							\
	chooser-icon.png						\
	xfsm-marshal.list
