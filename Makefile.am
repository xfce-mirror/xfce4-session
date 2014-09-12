SUBDIRS =								\
	libxfsm								\
	doc								\
	engines								\
	icons								\
	po								\
	settings							\
	scripts								\
	xfce4-session							\
	xfce4-session-logout						\
	xfsm-shutdown-helper

desktopdir = $(XSESSION_PREFIX)/share/xsessions
desktop_in_files = xfce.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

distuninstallcheck_listfiles = 						\
	find . -type f -print | grep -v ./share/icons/hicolor/icon-theme.cache

EXTRA_DIST =								\
	BUGS								\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	$(config_DATA)							\
	$(desktop_in_files)

DISTCLEANFILES =							\
	intltool-extract						\
	intltool-merge							\
	intltool-update							\
	$(desktop_DATA)

distclean-local:
	rm -rf *.cache *~

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

DISTCHECK_CONFIGURE_FLAGS =						\
	--with-xsession-prefix=/tmp/$(PACKAGE)-distcheck

# vi:set ts=8 sw=8 noet ai nocindent:
