SUBDIRS =								\
	defaults							\
	docs								\
	po								\
	settings							\
	xfce4-session							\
	xfce4-shutdown

distclean-local:
	rm -rf xfce4-session.spec *.cache *~

EXTRA_DIST =								\
	AUTHORS								\
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
