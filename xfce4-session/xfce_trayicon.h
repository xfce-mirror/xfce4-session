/*
 * Copyright (c) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is a wrapper class to NetkTrayIcon.
 */

#ifndef __XFCE_TRAY_ICON_H__
#define __XFCE_TRAY_ICON_H__

#include <libxfcegui4/netk-trayicon.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define	XFCE_TRAY_ICON(obj)						\
	G_TYPE_CHECK_INSTANCE_CAST(obj, xfce_tray_icon_get_type(),	\
			XfceTrayIcon)
#define	XFCE_TRAY_ICON_CLASS(klass)					\
	G_TYPE_CHECK_CLASS_CAST(klass, xfce_tray_icon_get_type(),	\
			XfceTrayIconClass)
#define	XFCE_IS_TRAY_ICON(obj)						\
	G_TYPE_CHECK_INSTANCE_TYPE(obj, xfce_tray_icon_get_type())

typedef struct	_XfceTrayIconClass	XfceTrayIconClass;
typedef struct	_XfceTrayIcon		XfceTrayIcon;

struct _XfceTrayIconClass
{
	GtkObjectClass		parent_class;

	/* user double-clicked the tray icon */
	void (*clicked)(XfceTrayIcon *);
};

struct _XfceTrayIcon
{
	GtkObject		object;

	/* */
	GtkWidget		*tray;

	/* */
	GtkWidget		*image;

	/* */
	GtkWidget		*menu;

	/* Tooltips */
	GtkTooltips		*tooltips;
};

/* prototypes */
extern GType		xfce_tray_icon_get_type(void);
extern XfceTrayIcon	*xfce_tray_icon_new(void);
extern XfceTrayIcon	*xfce_tray_icon_new_with_menu_from_pixbuf(GtkWidget *,
				GdkPixbuf *);
extern GtkWidget	*xfce_tray_icon_get_menu(XfceTrayIcon *);
extern void		xfce_tray_icon_set_menu(XfceTrayIcon *, GtkWidget *);
extern void		xfce_tray_icon_set_from_pixbuf(XfceTrayIcon *,
				GdkPixbuf *);
extern void		xfce_tray_icon_set_tooltip(XfceTrayIcon *,
				const gchar *, const gchar *);

G_END_DECLS

#endif	/* !__XFCE_TRAY_ICON_H__ */
