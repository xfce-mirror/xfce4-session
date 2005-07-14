# $Id$

INCLUDES =								\
	-I$(top_srcdir)							\
	-DG_LOG_DOMAIN=\"xfce4-autostart-editor\"			\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

bin_PROGRAMS =								\
	xfce4-autostart-editor

xfce4_autostart_editor_SOURCES =					\
	main.c								\
	xfae-dialog.c							\
	xfae-dialog.h							\
	xfae-model.c							\
	xfae-model.h							\
	xfae-window.c							\
	xfae-window.h

xfce4_autostart_editor_CFLAGS =						\
	@LIBXFCEGUI4_CFLAGS@

xfce4_autostart_editor_LDADD =						\
	@LIBXFCEGUI4_LIBS@

desktopdir = $(datadir)/applications
desktop_in_files = xfce4-autostart-editor.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	$(desktop_in_files)

DISTCLEANFILES =							\
	$(desktop_DATA)
# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
