bin_SCRIPTS= \
	startxfce4 \
	xflock4

# cannot be set from configure, because ${prefix} is not expanded
edit = $(SED) \
	-e "s,@_datadir_\@,$(datadir),g" \
	-e "s,@_libdir_\@,$(libdir),g" \
	-e "s,@_docdir_\@,$$(dirname $(docdir)),g" \
	-e "s,@_sysconfdir_\@,$(sysconfdir),g"

startxfce4: Makefile $(srcdir)/startxfce4.in
	rm -f startxfce4 startxfce4.tmp
	$(edit) $(srcdir)/startxfce4.in >startxfce4.tmp
	mv startxfce4.tmp startxfce4

xinitrc: Makefile xinitrc.in
	rm -f xinitrc xinitrc.tmp
	$(edit) xinitrc.in >xinitrc.tmp
	mv xinitrc.tmp xinitrc

conffilesdir=@sysconfdir@/xdg/xfce4
conffiles_DATA = \
	xinitrc \
	Xft.xrdb

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = xscreensaver.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = \
	$(autostart_in_files) \
	startxfce4.in \
	xinitrc.in \
	Xft.xrdb \
	xflock4

DISTCLEANFILES = \
	$(autostart_DATA) \
	startxfce4 \
	xinitrc
