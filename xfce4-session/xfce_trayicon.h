/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *                                                                              
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *                                                                              
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
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
	gchar			*tip_text;
	gchar			*tip_private;

	/* */
	gulong			destroyId;
};

/* prototypes */
extern GType		xfce_tray_icon_get_type(void);
extern XfceTrayIcon	*xfce_tray_icon_new(void);
extern XfceTrayIcon	*xfce_tray_icon_new_with_menu_from_pixbuf(GtkWidget *,
				GdkPixbuf *);
extern void		xfce_tray_icon_connect(XfceTrayIcon *);
extern void		xfce_tray_icon_disconnect(XfceTrayIcon *);
extern GtkWidget	*xfce_tray_icon_get_menu(XfceTrayIcon *);
extern void		xfce_tray_icon_set_menu(XfceTrayIcon *, GtkWidget *);
extern void		xfce_tray_icon_set_from_pixbuf(XfceTrayIcon *,
				GdkPixbuf *);
extern void		xfce_tray_icon_set_tooltip(XfceTrayIcon *,
				const gchar *, const gchar *);

G_END_DECLS

#endif	/* !__XFCE_TRAY_ICON_H__ */
