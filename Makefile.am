SUBDIRS =								\
	defaults							\
	docs								\
	icons								\
	po								\
	settings							\
	themes								\
	xfce4-session							\
	xfsm-shutdown-helper

configdir = $(sysconfdir)/xdg/xfce4-session
config_DATA = xfce4-session.rc

distclean-local:
	rm -rf xfce4-session.spec *.cache *~

EXTRA_DIST =								\
	$(config_DATA)							\
	AUTHORS								\
	BUGS								\
	ChangeLog.pre-xfce-devel					\
	INSTALL								\
	NEWS								\
	README								\
	TODO								\
	xfce4-session.spec						\
	xfce4-session.spec.in

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

ACLOCAL_AMFLAGS = -I m4
