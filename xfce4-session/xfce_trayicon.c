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

#include <gdk/gdkx.h>

#include <xfce_trayicon.h>

/* signals provided by the tray icon */
enum
{
	CLICKED,		/* user double-clicked the icon */
	LAST_SIGNAL
};

/* static prototypes */
static void	xfce_tray_icon_class_init(XfceTrayIconClass *);
static void	xfce_tray_icon_init(XfceTrayIcon *);
static void	xfce_tray_icon_finalize(GObject *);
static void	xfce_tray_icon_reconnect(XfceTrayIcon *);
static gboolean	xfce_tray_icon_disconnect(GtkWidget *, XfceTrayIcon *);
static void	xfce_tray_icon_press(GtkWidget *, GdkEventButton *,
			XfceTrayIcon *);
static void	xfce_tray_icon_popup(GtkWidget *, XfceTrayIcon *);

/* parent class */
static GObjectClass	*parent_class;

/* tray icon signals */
static guint 		tray_signals[LAST_SIGNAL];

/*
 */
GType
xfce_tray_icon_get_type(void)
{
	static GType tray_icon_type = 0;

	if (!tray_icon_type) {
		static const GTypeInfo tray_icon_info = {
			sizeof(XfceTrayIconClass),
			NULL,
			NULL,
			(GClassInitFunc)xfce_tray_icon_class_init,
			NULL,
			NULL,
			sizeof(XfceTrayIcon),
			0,
			(GInstanceInitFunc)xfce_tray_icon_init
		};

		tray_icon_type = g_type_register_static(
				GTK_TYPE_OBJECT,
				"XfceTrayIcon",
				&tray_icon_info,
				0);
	}

	return(tray_icon_type);
}

/*
 */
static void
xfce_tray_icon_class_init(XfceTrayIconClass *klass)
{
	GObjectClass *gobject_class;

	parent_class = gtk_type_class(GTK_TYPE_OBJECT);

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

	/* no icon image set */
	icon->image = gtk_image_new();
	g_object_ref(G_OBJECT(icon->image));
	gtk_widget_show(icon->image);

	/* create tooltips */
	icon->tooltips = gtk_tooltips_new();
	g_object_ref(G_OBJECT(icon->tooltips));
	gtk_object_sink(GTK_OBJECT(icon->tooltips));

	/* connect to system tray */
	(void)xfce_tray_icon_reconnect(icon);
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

	G_OBJECT_CLASS(parent_class)->finalize(object);
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

	/* */
	(void)g_signal_connect(G_OBJECT(icon->tray), "button_press_event",
			G_CALLBACK(xfce_tray_icon_press), icon);
	(void)g_signal_connect(G_OBJECT(icon->tray), "destroy",
			G_CALLBACK(xfce_tray_icon_disconnect), icon);
	(void)g_signal_connect(G_OBJECT(icon->tray), "popup_menu",
			G_CALLBACK(xfce_tray_icon_popup), icon);
}

/*
 */
static void
xfce_tray_icon_reconnect(XfceTrayIcon *icon)
{
	icon->tray = netk_tray_icon_new(GDK_SCREEN_XSCREEN(
				gdk_screen_get_default()));
	(void)g_signal_connect(G_OBJECT(icon->tray), "embedded",
			G_CALLBACK(xfce_tray_icon_embedded), icon);
	gtk_widget_show(icon->tray);
}

/*
 * The tray just disappeared
 */
static gboolean
xfce_tray_icon_disconnect(GtkWidget *tray, XfceTrayIcon *icon)
{
	GtkTooltipsData *data;
	gchar *private;
	gchar *text;

	/* */
	private = NULL;
	text = NULL;

	/* remember tooltips */
	if ((data = gtk_tooltips_data_get(tray)) != NULL) {
		if (data->tip_private != NULL)
			private = g_strdup(data->tip_private);

		if (data->tip_text != NULL)
			text = g_strdup(data->tip_text);
	}

	/* remove us from the tray icon */
	gtk_container_remove(GTK_CONTAINER(tray), icon->image);

	/* */
	xfce_tray_icon_reconnect(icon);

	/* set tooltip */
	if (private != NULL || text != NULL) {
		gtk_tooltips_set_tip(icon->tooltips, icon->tray, text, private);

		if (private != NULL)
			g_free(private);

		if (text != NULL)
			g_free(text);
	}

	/* keep the destroy going... */
	return(TRUE);
}

/*
 */
/* ARGSUSED */
static void
xfce_tray_icon_press(GtkWidget *widget, GdkEventButton *event,
                     XfceTrayIcon *icon)
{
	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
		g_signal_emit(G_OBJECT(icon), tray_signals[CLICKED], 0);
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

	gtk_tooltips_set_tip(icon->tooltips, icon->tray, text, private);
}

