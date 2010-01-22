# $Id$

INCLUDES =								\
	-DG_LOG_DOMAIN=\"xfce4-tips\"					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DTIPSDIR=\"$(datadir)/xfce4/tips\"				\
	-I$(top_srcdir)

SUBDIRS =								\
	data

bin_PROGRAMS =								\
	xfce4-tips

xfce4_tips_SOURCES =							\
	main.c

xfce4_tips_CFLAGS =							\
	@LIBXFCE4UI_CFLAGS@

xfce4_tips_LDADD =							\
	@LIBXFCE4UI_LIBS@

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = xfce4-tips-autostart.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	$(desktop_in_files)

DISTCLEANFILES =							\
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
