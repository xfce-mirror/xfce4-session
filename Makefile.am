ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS =								\
	libxfsm								\
	doc								\
	icons								\
	labwc								\
	po								\
	settings							\
	scripts								\
	xfce4-session							\
	xfce4-session-logout						\
	xfsm-shutdown-helper

desktop_in_files = xfce.desktop.in
desktop_wayland_in_files = xfce-wayland.desktop.in
DISTCLEANFILES =
EXTRA_DIST = \
	$(desktop_in_files) \
	$(desktop_wayland_in_files)

if ENABLE_X11

desktopdir = $(XSESSION_PREFIX)/share/xsessions
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

DISTCLEANFILES +=							\
	$(desktop_DATA)

endif

if ENABLE_WAYLAND

# do not rename xfce-wayland.desktop to xfce.desktop when installing:
# some DM like LightDM (+ GTK greeter) do not support "duplicate" entries
# i.e. the same basename in /usr/share/xsessions and /usr/share/wayland-sessions
desktop_waylanddir = $(WAYLAND_SESSION_PREFIX)/share/wayland-sessions
desktop_wayland_DATA = $(desktop_wayland_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

DISTCLEANFILES +=							\
	$(desktop_wayland_DATA)

endif

distuninstallcheck_listfiles = 						\
	find . -type f -print | grep -v ./share/icons/hicolor/icon-theme.cache

distclean-local:
	rm -rf *.cache *~

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

AM_DISTCHECK_CONFIGURE_FLAGS =						\
	--with-xsession-prefix=/tmp/$(PACKAGE)-distcheck \
	--with-wayland-session-prefix=/tmp/$(PACKAGE)-distcheck

# vi:set ts=8 sw=8 noet ai nocindent:
