/* $Id$ */
/*-
 * Copyright (c) 2003,2004 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* !HAVE_CONFIG_H */

#include <X11/Xlib.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <xfce_trayicon.h>

/* signals provided by the tray icon */
enum
{
	CLICKED,		/* user single-clicked the icon */
	DBL_CLICKED,		/* user double-clicked the icon */
	LAST_SIGNAL
};

/* static prototypes */
static void	xfce_tray_icon_finalize(GObject *);
static void	xfce_tray_icon_reconnect(XfceTrayIcon *);
static void	xfce_tray_icon_destroy(GtkWidget *, XfceTrayIcon *);
static void	xfce_tray_icon_press(GtkWidget *, GdkEventButton *,
			XfceTrayIcon *);
static void	xfce_tray_icon_popup(GtkWidget *, XfceTrayIcon *);

/* tray icon signals */
static guint 		tray_signals[LAST_SIGNAL];


G_DEFINE_TYPE (XfceTrayIcon, xfce_tray_icon, GTK_TYPE_OBJECT)
}

/*
 */
static void
xfce_tray_icon_class_init(XfceTrayIconClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS(klass);
	gobject_class->finalize = xfce_tray_icon_finalize;

	/*
	 * create signals
	 */
	tray_signals[CLICKED] = g_signal_new("clicked",
			G_OBJECT_CLASS_TYPE(klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(XfceTrayIconClass, clicked),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE,
			0);
	tray_signals[DBL_CLICKED] = g_signal_new("double_clicked",
			G_OBJECT_CLASS_TYPE(klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET(XfceTrayIconClass, clicked),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE,
			0);
}

/*
 */
static void
xfce_tray_icon_init(XfceTrayIcon *icon)
{
	/* no menu connected */
	icon->menu = NULL;

	/* */
	icon->tray = NULL;

	/* */
	icon->tip_text = NULL;
	icon->tip_private = NULL;

	/* */
	icon->destroyId = 0;

	/* no icon image set */
	icon->image = gtk_image_new();
	g_object_ref(G_OBJECT(icon->image));
	gtk_widget_show(icon->image);

	/* create tooltips */
	icon->tooltips = gtk_tooltips_new();
	g_object_ref(G_OBJECT(icon->tooltips));
	gtk_object_sink(GTK_OBJECT(icon->tooltips));
}

/*
 */
static void
xfce_tray_icon_finalize(GObject *object)
{
	XfceTrayIcon *icon;

	g_return_if_fail(XFCE_IS_TRAY_ICON(object));

	icon = XFCE_TRAY_ICON(object);
	gtk_widget_destroy(icon->image);
	g_object_unref(G_OBJECT(icon->image));
	g_object_unref(G_OBJECT(icon->tooltips));

	if (icon->tip_text != NULL)
		g_free(icon->tip_text);
	if (icon->tip_private != NULL)
		g_free(icon->tip_private);

	G_OBJECT_CLASS(xfce_tray_icon_parent_class)->finalize(object);
}

/*
 */
/* ARGUSED */
static void
xfce_tray_icon_embedded(GtkWidget *widget, XfceTrayIcon *icon)
{
	gtk_widget_add_events(icon->tray, GDK_BUTTON_PRESS_MASK |
			GDK_BUTTON_RELEASE_MASK);

	/* */
	gtk_container_add(GTK_CONTAINER(icon->tray), icon->image);

	/* add tooltips */
	gtk_tooltips_set_tip(icon->tooltips, icon->tray, icon->tip_text,
			icon->tip_private);

	/* */
	(void)g_signal_connect(G_OBJECT(icon->tray), "button_press_event",
			G_CALLBACK(xfce_tray_icon_press), icon);
	icon->destroyId = g_signal_connect(G_OBJECT(icon->tray), "destroy",
			G_CALLBACK(xfce_tray_icon_destroy), icon);
	(void)g_signal_connect(G_OBJECT(icon->tray), "popup_menu",
			G_CALLBACK(xfce_tray_icon_popup), icon);
}

/*
 */
static void
xfce_tray_icon_reconnect(XfceTrayIcon *icon)
{
	icon->tray = netk_tray_icon_new(DefaultScreenOfDisplay(GDK_DISPLAY()));
	(void)g_signal_connect(G_OBJECT(icon->tray), "embedded",
			G_CALLBACK(xfce_tray_icon_embedded), icon);
	gtk_widget_show(icon->tray);
}

/*
 * The tray just disappeared
 */
static void
xfce_tray_icon_destroy(GtkWidget *tray, XfceTrayIcon *icon)
{
	/* destroy handler was fired, so the destroyId is no longer valid */
	icon->destroyId = 0;

	/* remove us from the tray icon */
	gtk_container_remove(GTK_CONTAINER(tray), icon->image);

	/* */
	xfce_tray_icon_reconnect(icon);
}

/*
 */
/* ARGSUSED */
static void
xfce_tray_icon_press(GtkWidget *widget, GdkEventButton *event,
                     XfceTrayIcon *icon)
{
	if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
		g_signal_emit(G_OBJECT(icon), tray_signals[CLICKED], 0);
	}
	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
		g_signal_emit(G_OBJECT(icon), tray_signals[DBL_CLICKED], 0);
	}
	else if (event->button == 3 && GTK_IS_MENU(icon->menu)) {
		gtk_menu_popup(GTK_MENU(icon->menu), NULL, NULL, NULL, NULL,
				event->button, gtk_get_current_event_time());
	}
}

/*
 */
/* ARGSUSED */
static void
xfce_tray_icon_popup(GtkWidget *widget, XfceTrayIcon *icon)
{
	g_return_if_fail(GTK_IS_MENU(icon->menu));

	gtk_menu_popup(GTK_MENU(icon->menu), NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time());
}

/*
 */
XfceTrayIcon *
xfce_tray_icon_new(void)
{
	return(XFCE_TRAY_ICON(g_object_new(xfce_tray_icon_get_type(), NULL)));
}

/*
 */
XfceTrayIcon *
xfce_tray_icon_new_with_menu_from_pixbuf(GtkWidget *menu, GdkPixbuf *pixbuf)
{
	XfceTrayIcon *icon;

	icon = XFCE_TRAY_ICON(g_object_new(xfce_tray_icon_get_type(), NULL));

	xfce_tray_icon_set_menu(icon, menu);
	xfce_tray_icon_set_from_pixbuf(icon, pixbuf);

	return(icon);
}

/*
 */
void
xfce_tray_icon_connect(XfceTrayIcon *icon)
{
	g_return_if_fail(XFCE_IS_TRAY_ICON(icon));

	/* check if icon is not already connected */
	if (!NETK_IS_TRAY_ICON(icon->tray))
		xfce_tray_icon_reconnect(icon);
}

/*
 */
void
xfce_tray_icon_disconnect(XfceTrayIcon *icon)
{
	g_return_if_fail(XFCE_IS_TRAY_ICON(icon));

	/* check if icon is already connected */
	if (NETK_IS_TRAY_ICON(icon->tray)) {
		/* disconnect destroy handler (REQUIRED)! */
		if (icon->destroyId != 0) {
			g_signal_handler_disconnect(G_OBJECT(icon->tray),
					icon->destroyId);
			icon->destroyId = 0;
		}

		if (gtk_bin_get_child(GTK_BIN(icon->tray)) == icon->image) {
			gtk_container_remove(GTK_CONTAINER(icon->tray),
					icon->image);
		}

		gtk_widget_destroy(icon->tray);
		icon->tray = NULL;
	}
}

/*
 */
GtkWidget *
xfce_tray_icon_get_menu(XfceTrayIcon *icon)
{
	g_return_val_if_fail(XFCE_IS_TRAY_ICON(icon), NULL);

	return(icon->menu);
}

/*
 */
void
xfce_tray_icon_set_menu(XfceTrayIcon *icon, GtkWidget *menu)
{
	g_return_if_fail(GTK_IS_MENU(menu));
	g_return_if_fail(XFCE_IS_TRAY_ICON(icon));

	icon->menu = menu;
}

/*
 */
void
xfce_tray_icon_set_from_pixbuf(XfceTrayIcon *icon, GdkPixbuf *pixbuf)
{
	g_return_if_fail(XFCE_IS_TRAY_ICON(icon));

	/* set new pixbuf */
	gtk_image_set_from_pixbuf(GTK_IMAGE(icon->image), pixbuf);
}

/*
 */
void
xfce_tray_icon_set_tooltip(XfceTrayIcon *icon, const gchar *text,
                           const gchar *private)
{
	g_return_if_fail(XFCE_IS_TRAY_ICON(icon));

	if (icon->tip_text != NULL) {
		g_free(icon->tip_text);
		icon->tip_text = NULL;
	}

	if (icon->tip_private != NULL) {
		g_free(icon->tip_private);
		icon->tip_private = NULL;
	}

	if (NETK_IS_TRAY_ICON(icon->tray))
		gtk_tooltips_set_tip(icon->tooltips, icon->tray, text, private);

	if (text != NULL)
		icon->tip_text = g_strdup(text);

	if (private != NULL)
		icon->tip_private = g_strdup(private);
}

