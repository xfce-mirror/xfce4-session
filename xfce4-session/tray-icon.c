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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* !HAVE_CONFIG_H */

#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4util/i18n.h>

#include <settings/session-icon.h>
#include <xfce4-session/tray-icon.h>

/* static prototypes */
static void	xfsm_tray_icon_class_init(XfsmTrayIconClass *);
static void	xfsm_tray_icon_init(XfsmTrayIcon *);
static void	xfsm_tray_icon_finalize(GObject *);

/* parent class */
static GObjectClass	*parent_class;

/*
 */
GType
xfsm_tray_icon_get_type(void)
{
	static GType tray_icon_type = 0;

	if (!tray_icon_type) {
		static const GTypeInfo tray_icon_info = {
			sizeof(XfsmTrayIconClass),
			NULL,
			NULL,
			(GClassInitFunc)xfsm_tray_icon_class_init,
			NULL,
			NULL,
			sizeof(XfsmTrayIcon),
			0,
			(GInstanceInitFunc)xfsm_tray_icon_init
		};

		tray_icon_type = g_type_register_static(
				netk_tray_icon_get_type(),
				"XfsmTrayIcon",
				&tray_icon_info,
				0);
	}

	return(tray_icon_type);
}

/*
 */
static void
xfsm_tray_icon_class_init(XfsmTrayIconClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS(klass);
	gobject_class->finalize = xfsm_tray_icon_finalize;
	parent_class = gtk_type_class(netk_tray_icon_get_type());
}

/*
 */
/* ARGSUSED */
static void
xfsm_tray_icon_press(XfsmTrayIcon *icon, GdkEventButton *event, gpointer data)
{
	/* check if menu is connected */
	g_return_if_fail(GTK_IS_MENU(icon->menu));

	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
		/* check if menu item is connected */
		if (!GTK_IS_MENU_ITEM(icon->defaultItem))
			return;

		gtk_menu_shell_activate_item(GTK_MENU_SHELL(icon->menu),
				icon->defaultItem, TRUE);
	}
	else if (event->button == 3) {
		gtk_menu_popup(icon->menu, NULL, NULL, NULL, NULL,
			event->button, gtk_get_current_event_time());
	}
}

/*
 */
static void
xfsm_tray_icon_popup(XfsmTrayIcon *icon, gpointer data)
{
	gtk_menu_popup(GTK_MENU(icon->menu), NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time());
}

/*
 */
static void
xfsm_tray_icon_init(XfsmTrayIcon *icon)
{
	GtkWidget *eventbox;
	GdkPixbuf *pixbuf;
	GtkWidget *image;

	/* no menu connected yet */
	icon->menu = NULL;
	icon->defaultItem = NULL;

	/* set default screen for tray icon */
	netk_tray_icon_set_screen(NETK_TRAY_ICON(icon),
		GDK_SCREEN_XSCREEN(gdk_screen_get_default()));

	/* listen for the required events */
	gtk_widget_add_events(GTK_WIDGET(icon), GDK_BUTTON_PRESS_MASK | 
			GDK_FOCUS_CHANGE_MASK);

	/* */
	eventbox = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(icon), eventbox);
	gtk_widget_show(eventbox);

	/* */
	pixbuf = inline_icon_at_size(session_icon_data, 16, 16);
	image = gtk_image_new_from_pixbuf(pixbuf);
	gtk_container_add(GTK_CONTAINER(eventbox), image);
	g_object_unref(pixbuf);
	gtk_widget_show(image);

	/* */
	g_signal_connect(G_OBJECT(icon), "button_press_event",
			G_CALLBACK(xfsm_tray_icon_press), NULL);
	g_signal_connect(G_OBJECT(icon), "popup_menu",
			G_CALLBACK(xfsm_tray_icon_popup), NULL);
}

/*
 */
static void
xfsm_tray_icon_finalize(GObject *object)
{
	XfsmTrayIcon *icon;

	g_return_if_fail(XFSM_IS_TRAY_ICON(object));

	icon = XFSM_TRAY_ICON(object);
	
	g_object_unref(G_OBJECT(icon->menu));
	g_object_unref(G_OBJECT(icon->defaultItem));

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/*
 */
GtkWidget *
xfsm_tray_icon_new(GtkMenu *menu, GtkWidget *defaultItem)
{
	XfsmTrayIcon *icon;

	icon = XFSM_TRAY_ICON(g_object_new(xfsm_tray_icon_get_type(), NULL));
	icon->menu = GTK_MENU(g_object_ref(menu));
	icon->defaultItem = GTK_WIDGET(g_object_ref(defaultItem));
	
	return(GTK_WIDGET(icon));
}

