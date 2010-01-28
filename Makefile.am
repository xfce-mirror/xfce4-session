# $Id$

SUBDIRS =								\
	libxfsm								\
	doc								\
	engines								\
	icons								\
	po								\
	settings							\
	xfce4-session							\
	xfce4-session-logout						\
	xfce4-tips							\
	xfsm-shutdown-helper						\
	$(panel_plugin_dir)

if HAVE_LIBXFCE4PANEL
panel_plugin_dir = panel-plugin
endif

EXTRA_DIST =								\
	BUGS								\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	xfce4-session.spec						\
	$(config_DATA)

DISTCLEANFILES =							\
	intltool-extract						\
	intltool-merge							\
	intltool-update

html:
	(cd doc && $(MAKE) $(AM_MAKEFLAGS) html)

AUTOMAKE_OPTIONS =							\
	1.8								\
	dist-bzip2

distclean-local:
	rm -rf xfce4-session.spec *.cache *~

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

# vi:set ts=8 sw=8 noet ai nocindent:
