ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS =								\
	libxfsm								\
	doc								\
	icons								\
	po								\
	settings							\
	scripts								\
	xfce4-session							\
	xfce4-session-logout						\
	xfsm-shutdown-helper

if ENABLE_X11

desktopdir = $(XSESSION_PREFIX)/share/xsessions
desktop_in_files = xfce.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST =								\
	$(desktop_in_files)

DISTCLEANFILES =							\
	$(desktop_DATA)

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
	--with-xsession-prefix=/tmp/$(PACKAGE)-distcheck

# vi:set ts=8 sw=8 noet ai nocindent:
