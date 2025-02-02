
AM_CPPFLAGS =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/libxfsm                                         \
	-DG_LOG_DOMAIN=\"xfce4-session\"				\
	-DLIBDIR=\"$(libdir)\"						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DSYSCONFDIR=\"$(sysconfdir)\"					\
	-DXFSM_SHUTDOWN_HELPER_CMD=\"$(HELPER_PATH_PREFIX)/xfce4/session/xfsm-shutdown-helper\" \
	-DDBUS_API_SUBJECT_TO_CHANGE					\
	$(PLATFORM_CPPFLAGS)

man_MANS = xfce4-session.1

bin_PROGRAMS = xfce4-session

xfce4_session_built_sources =						\
	xfsm-chooser-icon.h						\
	xfsm-client-dbus.c						\
	xfsm-client-dbus.h						\
	xfsm-manager-dbus.c						\
	xfsm-manager-dbus.h						\
	xfsm-marshal.c							\
	xfsm-marshal.h

xfce4_session_SOURCES =							\
	$(xfce4_session_built_sources)					\
	main.c								\
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
	xfsm-global.c							\
	xfsm-global.h							\
	xfsm-inhibition.c						\
	xfsm-inhibition.h						\
	xfsm-inhibitor.c						\
	xfsm-inhibitor.h						\
	xfsm-logout-dialog.c						\
	xfsm-logout-dialog.h						\
	xfsm-manager.c							\
	xfsm-manager.h							\
	xfsm-packagekit.c						\
	xfsm-packagekit.h						\
	xfsm-properties.c						\
	xfsm-properties.h						\
	xfsm-shutdown-fallback.c				\
	xfsm-shutdown-fallback.h				\
	xfsm-shutdown.c							\
	xfsm-shutdown.h							\
	xfsm-startup.c							\
	xfsm-startup.h

xfce4_session_CFLAGS =							\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(GMODULE_CFLAGS)						\
	$(GIO_CFLAGS)							\
	$(GIO_UNIX_CFLAGS)						\
	$(POLKIT_CFLAGS)						\
	$(XFCONF_CFLAGS)						\
	$(LIBXFCE4WINDOWING_CFLAGS)						\
	$(GTK_LAYER_SHELL_CFLAGS)						\
	$(PLATFORM_CFLAGS)

xfce4_session_LDFLAGS =							\
	-no-undefined							\
	$(PLATFORM_LDFLAGS)

xfce4_session_LDADD =							\
	$(top_builddir)/libxfsm/libxfsm.la				\
	$(LIBXFCE4UI_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)						\
	$(GMODULE_LIBS)							\
	$(GIO_LIBS)								\
	$(GIO_UNIX_LIBS)						\
	$(POLKIT_LIBS)							\
	$(XFCONF_LIBS)							\
	$(LIBXFCE4WINDOWING_LIBS)							\
	$(GTK_LAYER_SHELL_LIBS)							\
	$(LIBM)

if ENABLE_X11
xfce4_session_SOURCES += \
	ice-layer.c \
	ice-layer.h \
	sm-layer.c \
	sm-layer.h \
	xfsm-fadeout.c \
	xfsm-fadeout.h

if ENABLE_LEGACY_SESSION_MANAGEMENT
xfce4_session_SOURCES += \
	xfsm-legacy.c \
	xfsm-legacy.h
endif

xfce4_session_CFLAGS +=\
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE \
	$(LIBX11_CFLAGS) \
	$(LIBWNCK_CFLAGS) \
	$(LIBICE_CFLAGS) \
	$(LIBSM_CFLAGS) \
	$(PLATFORM_CFLAGS)

xfce4_session_LDADD += \
	$(LIBX11_LIBS) \
	$(LIBWNCK_LIBS) \
	$(LIBICE_LIBS) \
	$(LIBSM_LIBS)
endif

xdg_desktop_portaldir = $(datadir)/xdg-desktop-portal
dist_xdg_desktop_portal_DATA = xfce-portals.conf

if HAVE_POLKIT

%.policy: %.policy.in
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_DATA =					\
	org.xfce.session.policy

%.policy.in: %.policy.in.in
	sed "s|[@]HELPER_PATH_PREFIX@|${HELPER_PATH_PREFIX}|" $< > $@

endif

xfsm-chooser-icon.h: $(srcdir)/xfsm-chooser-icon.png
	$(AM_V_GEN) gdk-pixbuf-csource --static --raw --stream --name=xfsm_chooser_icon_data $< > $@

xfsm-marshal.h: $(srcdir)/xfsm-marshal.list Makefile
	$(AM_V_GEN) glib-genmarshal --prefix=xfsm_marshal --internal --header $< > $@

xfsm-marshal.c: $(srcdir)/xfsm-marshal.list Makefile
	$(AM_V_GEN) echo "#include <xfce4-session/xfsm-marshal.h>" > $@ \
	&& glib-genmarshal --prefix=xfsm_marshal --body $< >> $@

xfsm-client-dbus.c xfsm-client-dbus.h : $(srcdir)/xfsm-client-dbus.xml Makefile.am
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--c-namespace=XfsmDbus \
		--interface-prefix=org.xfce.Session. \
		--generate-c-code=xfsm-client-dbus \
		$(srcdir)/xfsm-client-dbus.xml

xfsm-manager-dbus.c xfsm-manager-dbus.h : $(srcdir)/xfsm-manager-dbus.xml Makefile.am
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--c-namespace=XfsmDbus \
		--interface-prefix=org.xfce.Session. \
		--generate-c-code=xfsm-manager-dbus \
		$(srcdir)/xfsm-manager-dbus.xml

BUILT_SOURCES =								\
	$(xfce4_session_built_sources)

EXTRA_DIST =								\
	$(man_MANS)							\
	xfsm-chooser-icon.png						\
	xfsm-marshal.list						\
	xfsm-client-dbus.xml						\
	xfsm-manager-dbus.xml					\
	org.xfce.session.policy.in.in

DISTCLEANFILES =							\
	$(BUILT_SOURCES)				\
	org.xfce.session.policy					\
	org.xfce.session.policy.in

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
