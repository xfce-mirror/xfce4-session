
AM_CPPFLAGS =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DG_LOG_DOMAIN=\"xfce4-session\"				\
	-DLIBDIR=\"$(libdir)\"						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DSYSCONFDIR=\"$(sysconfdir)\"					\
	-DXFSM_SHUTDOWN_HELPER_CMD=\"$(HELPER_PATH_PREFIX)/xfce4/session/xfsm-shutdown-helper\" \
	-DDBUS_API_SUBJECT_TO_CHANGE					\
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE					\
	-DUPOWER_ENABLE_DEPRECATED 						\
	$(PLATFORM_CPPFLAGS)

man_MANS = xfce4-session.1

bin_PROGRAMS = xfce4-session

xfce4_session_built_sources =						\
	xfsm-chooser-icon.h						\
	xfsm-client-dbus.h						\
	xfsm-manager-dbus.h						\
	xfsm-marshal.c							\
	xfsm-marshal.h

xfce4_session_SOURCES =							\
	$(xfce4_session_built_sources)					\
	ice-layer.c							\
	ice-layer.h							\
	main.c								\
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
	xfsm-consolekit.c						\
	xfsm-consolekit.h						\
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
	xfsm-logout-dialog.c						\
	xfsm-logout-dialog.h						\
	xfsm-manager.c							\
	xfsm-manager.h							\
	xfsm-properties.c						\
	xfsm-properties.h						\
	xfsm-shutdown-fallback.c				\
	xfsm-shutdown-fallback.h				\
	xfsm-shutdown.c							\
	xfsm-shutdown.h							\
	xfsm-splash-screen.c						\
	xfsm-splash-screen.h						\
	xfsm-startup.c							\
	xfsm-startup.h							\
	xfsm-upower.c							\
	xfsm-upower.h							\
	xfsm-systemd.c							\
	xfsm-systemd.h


xfce4_session_CFLAGS =							\
	$(LIBSM_CFLAGS)							\
	$(LIBX11_CFLAGS)						\
	$(LIBXFCE4UI_CFLAGS)						\
	$(DBUS_CFLAGS)							\
	$(DBUS_GLIB_CFLAGS)						\
	$(LIBWNCK_CFLAGS)						\
	$(POLKIT_CFLAGS)						\
	$(XFCONF_CFLAGS)						\
	$(GMODULE_CFLAGS)						\
	$(PLATFORM_CFLAGS)						\
	$(UPOWER_CFLAGS)

xfce4_session_LDFLAGS =							\
	-no-undefined							\
	$(PLATFORM_LDFLAGS)

xfce4_session_LDADD =							\
	$(top_builddir)/libxfsm/libxfsm-4.6.la				\
	$(LIBSM_LDFLAGS)						\
	$(LIBSM_LIBS)							\
	$(LIBX11_LDFLAGS)						\
	$(LIBX11_LIBS)							\
	$(LIBXFCE4UI_LIBS)						\
	$(GMODULE_LIBS)							\
	$(DBUS_LIBS)							\
	$(DBUS_GLIB_LIBS)						\
	$(LIBWNCK_LIBS)							\
	$(POLKIT_LIBS)							\
	$(XFCONF_LIBS)							\
	$(UPOWER_LIBS)							\
	-lm

xfce4_session_DEPENDENCIES =						\
	$(top_builddir)/libxfsm/libxfsm-4.6.la

if HAVE_POLKIT

@INTLTOOL_POLICY_RULE@

polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_DATA =					\
	org.xfce.session.policy
.in2.in:
	sed "s|[@]HELPER_PATH_PREFIX@|${HELPER_PATH_PREFIX}|" $< > $@

endif

if MAINTAINER_MODE

xfsm-chooser-icon.h: $(srcdir)/xfsm-chooser-icon.png
	$(AM_V_GEN) gdk-pixbuf-csource --static --raw --stream --name=xfsm_chooser_icon_data $< > $@

xfsm-marshal.h: $(srcdir)/xfsm-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=xfsm_marshal --internal --header $< > $@

xfsm-marshal.c: $(srcdir)/xfsm-marshal.list Makefile
	$(AM_V_GEN) echo "#include <xfce4-session/xfsm-marshal.h>" > $@ \
	&& glib-genmarshal --prefix=xfsm_marshal --body $< >> $@

xfsm-manager-dbus.h: $(srcdir)/xfsm-manager-dbus.xml
	$(AM_V_GEN) dbus-binding-tool --mode=glib-server --prefix=xfsm_manager $< > $@

xfsm-client-dbus.h: $(srcdir)/xfsm-client-dbus.xml
	$(AM_V_GEN) dbus-binding-tool --mode=glib-server --prefix=xfsm_client $< > $@

BUILT_SOURCES =								\
	$(xfce4_session_built_sources)

endif

EXTRA_DIST =								\
	$(man_MANS)							\
	xfsm-chooser-icon.png						\
	xfsm-marshal.list						\
	xfsm-client-dbus.xml						\
	xfsm-manager-dbus.xml					\
	org.xfce.session.policy.in2

DISTCLEANFILES =							\
	$(xfce4_session_built_sources)			\
	org.xfce.session.policy					\
	org.xfce.session.policy.in

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
