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
	xfce4-autostart-editor						\
	xfce4-tips							\
	xfsm-shutdown-helper

configdir = $(sysconfdir)/xdg/xfce4-session
config_DATA = xfce4-session.rc

EXTRA_DIST =								\
	BUGS								\
	ChangeLog.pre-xfce-devel					\
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

# vi:set ts=8 sw=8 noet ai nocindent:
