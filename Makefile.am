@SET_MAKE@

SUBDIRS =								\
	libxfsm								\
	docs								\
	engines								\
	icons								\
	po								\
	settings							\
	xfce4-session							\
	xfce4-session-logout						\
	xfsm-shutdown-helper

configdir = $(sysconfdir)/xdg/xfce4-session
config_DATA = xfce4-session.rc

distclean-local:
	rm -rf xfce4-session.spec *.cache *~

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
	(cd docs && $(MAKE) $(AM_MAKEFLAGS) html)

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

dist-bz2: dist
	zcat $(PACKAGE)-$(VERSION).tar.gz | \
	 bzip2 --best -c > $(PACKAGE)-$(VERSION).tar.bz2

distcheck-bz2: distcheck
	zcat $(PACKAGE)-$(VERSION).tar.gz | \
	 bzip2 --best -c > $(PACKAGE)-$(VERSION).tar.bz2
