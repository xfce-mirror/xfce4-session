/* $Id$ */
/*-
 * Copyright (c) 2003,2004 Benedikt Meurer <benny@xfce.org>
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

#include <gtk/gtk.h>
#include <libxfce4util/i18n.h>
#include <libxfcegui4/libxfcegui4.h>

#include <settings/session-icon.h>
#include <xfce4-session/client.h>
#include <xfce4-session/session-control.h>
#include <xfce4-session/util.h>

/* default border */
#define BORDER		6

/* */
enum 
{
	PRIORITY_COLUMN,
	PROGRAM_COLUMN,
	USERID_COLUMN,
	STATE_COLUMN,
	N_COLUMNS
};

/* */
typedef struct
{
	Client		*client;
	GtkTreeIter	iter;
} ListItem;

#define LIST_ITEM(obj)		((ListItem *)(obj))

/* static prototypes */
static void	xfsm_session_control_class_init(XfsmSessionControlClass *);
static void	xfsm_session_control_init(XfsmSessionControl *);
static void	xfsm_session_control_finalize(GObject *);

/* parent class */
static GObjectClass	*parent_class;

/* client state names */
static const gchar *state_names[] = {
	N_("Idle"),
	N_("Interacting"),
	N_("Save completed"),
	N_("Saving"),			/* global save */
	N_("Saving (local)"),		/* local save */
	N_("Waiting to interact"),
	N_("Waiting to enter Phase2"),
	N_("Disconnecting"),
	NULL
};

/*
 */
GType
xfsm_session_control_get_type(void)
{
	static GType session_control_type = 0;

	if (!session_control_type) {
		static const GTypeInfo session_control_info = {
			sizeof(XfsmSessionControlClass),
			NULL,
			NULL,
			(GClassInitFunc)xfsm_session_control_class_init,
			NULL,
			NULL,
			sizeof(XfsmSessionControl),
			0,
			(GInstanceInitFunc)xfsm_session_control_init
		};

		session_control_type = g_type_register_static(GTK_TYPE_DIALOG,
				"XfsmSessionControl", &session_control_info, 0);
	}

	return(session_control_type);
}

/*
 */
static void
xfsm_session_control_class_init(XfsmSessionControlClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS(klass);
	gobject_class->finalize = xfsm_session_control_finalize;
	parent_class = gtk_type_class(gtk_dialog_get_type());
}

/*
 */
static ListItem *
xfsm_session_control_get_selected(XfsmSessionControl *list, GtkTreeSelection *selection)
{
	GtkTreeModel *store;
	GtkTreePath *cpath;
	GtkTreePath *spath;
	GtkTreeIter iter;
	ListItem *item;
	GList *lp;

	/* make gcc happy */
	item = NULL;

	if (gtk_tree_selection_get_selected(selection, &store, &iter)) {
		/* get path to selected item */
		spath = gtk_tree_model_get_path(store, &iter);

		for (lp = g_list_first(list->clients); lp; lp = lp->next) {
			item = LIST_ITEM(lp->data);
			cpath = gtk_tree_model_get_path(store, &item->iter);

			if (gtk_tree_path_compare(spath, cpath) == 0)
				break;

			gtk_tree_path_free(cpath);
		}
		
		/* no longer needed */
		gtk_tree_path_free(spath);

		if (lp != NULL)
			return(item);
	}

	return(NULL);
}

/*
 */
static void
xfsm_session_control_selection_changed(GtkTreeSelection *selection,
                                       XfsmSessionControl *control)
{
	ListItem *item;
	gchar **argv;

	if ((item = xfsm_session_control_get_selected(control, selection))
			!= NULL) {
		/* activate kill button */
		gtk_widget_set_sensitive(control->killButton, TRUE);

		/* check whether to activate clone button */
		if ((argv = client_get_command(item->client, SmCloneCommand))) {
			gtk_widget_set_sensitive(control->cloneButton, TRUE);
			g_strfreev(argv);
		}
		else
			gtk_widget_set_sensitive(control->cloneButton, FALSE);
	}
	else {
		/* no client selected */
		gtk_widget_set_sensitive(control->cloneButton, FALSE);
		gtk_widget_set_sensitive(control->killButton, FALSE);
	}
}

/*
 */
static gboolean
xfsm_session_control_clone_real(const Client *client)
{
	GError *error;

	error = NULL;

	if (!client_run_command(client, SmCloneCommand, &error)) {
		xfce_err(_("Unable to clone client: %s"), error->message);
		g_error_free(error);
	}

	return(FALSE);
}

/*
 */
static void
xfsm_session_control_clone(GtkWidget *button, XfsmSessionControl *list)
{
	GtkTreeSelection *selection;
	ListItem *item;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list->tree));

	if ((item = xfsm_session_control_get_selected(list, selection)) == NULL)
		return;

	g_idle_add((GSourceFunc)xfsm_session_control_clone_real, item->client);
}

/*
 */
static void
xfsm_session_control_kill(GtkWidget *button, XfsmSessionControl *list)
{
	GtkTreeSelection *selection;
	ListItem *item;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list->tree));

	if ((item = xfsm_session_control_get_selected(list, selection)) != NULL) {
		/* send SmDie to client */
		client_kill(item->client);
	}
}

/*
 * Delete event callback, just hide the window.
 */
/* ARGSUSED */
static gboolean
xfsm_session_control_delete(GtkWidget *widget, GdkEvent *ev, XfsmSessionControl *list)
{
	/* hide the dialog */
	gtk_widget_hide(GTK_WIDGET(list));
	return(TRUE);
}

/*
 */
static void
xfsm_session_control_init(XfsmSessionControl *list)
{
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkListStore *store;
	GdkPixbuf *icon;
	GtkWidget *swin;
	GtkWidget *vbox;

	/* start with an empty list of clients */
	list->clients = NULL;

	/* window options */
	gtk_window_set_position(GTK_WINDOW(list), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_title(GTK_WINDOW(list), _("Session control"));
	gtk_window_stick(GTK_WINDOW(list));

	/* set window icon */
	icon = gdk_pixbuf_new_from_inline(-1, session_icon_data, FALSE, NULL);
	gtk_window_set_icon(GTK_WINDOW(list), icon);
	g_object_unref(G_OBJECT(icon));

	/* */
	vbox = GTK_DIALOG(list)->vbox;

	/* create the list store */
	store = gtk_list_store_new(N_COLUMNS,
			G_TYPE_INT,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING);

	/* */
	list->tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list->tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	gtk_widget_set_size_request(list->tree, -1, 250);

	/* the view now holds a reference, we can get rid of this one */
	g_object_unref(G_OBJECT(store));

	/* add the columns */
	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "xalign", 1.0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list->tree),
		gtk_tree_view_column_new_with_attributes(_("Priority"),
			renderer,
			"text", PRIORITY_COLUMN,
			NULL));
	gtk_tree_view_append_column(GTK_TREE_VIEW(list->tree),
		gtk_tree_view_column_new_with_attributes(_("Program"),
			gtk_cell_renderer_text_new(),
			"text", PROGRAM_COLUMN,
			NULL));
	gtk_tree_view_append_column(GTK_TREE_VIEW(list->tree),
		gtk_tree_view_column_new_with_attributes(_("User"),
			gtk_cell_renderer_text_new(),
			"text", USERID_COLUMN,
			NULL));
	gtk_tree_view_append_column(GTK_TREE_VIEW(list->tree),
		gtk_tree_view_column_new_with_attributes(_("State"),
			gtk_cell_renderer_text_new(),
			"text", STATE_COLUMN,
			NULL));

	/* */
	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin),
			GTK_SHADOW_ETCHED_IN);
	gtk_box_pack_start(GTK_BOX(vbox), swin, TRUE, TRUE, BORDER);
	gtk_widget_show(swin);

	/* */
	gtk_container_add(GTK_CONTAINER(swin), list->tree);
	gtk_widget_show(list->tree);

	/* kill button */
	list->killButton = xfsm_imgbtn_new(_("Kill client"), GTK_STOCK_CANCEL);
	gtk_dialog_add_action_widget(GTK_DIALOG(list), list->killButton,
			GTK_RESPONSE_NONE);
	gtk_widget_show(list->killButton);

	/* clone button */
	list->cloneButton = xfsm_imgbtn_new(_("Clone client"), GTK_STOCK_COPY);
	gtk_dialog_add_action_widget(GTK_DIALOG(list), list->cloneButton,
			GTK_RESPONSE_NONE);
	gtk_widget_show(list->cloneButton);

	/* close button */
	list->closeButton = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	gtk_dialog_add_action_widget(GTK_DIALOG(list), list->closeButton,
			GTK_RESPONSE_NONE);
	gtk_widget_show(list->closeButton);

	/* */
	g_signal_connect(G_OBJECT(list->cloneButton), "clicked",
			G_CALLBACK(xfsm_session_control_clone), list);
	g_signal_connect(G_OBJECT(list->killButton), "clicked",
			G_CALLBACK(xfsm_session_control_kill), list);
	g_signal_connect(G_OBJECT(selection), "changed", 
			G_CALLBACK(xfsm_session_control_selection_changed), list);
	g_signal_connect_swapped(G_OBJECT(list->closeButton), "clicked",
			G_CALLBACK(gtk_widget_hide), list);
	g_signal_connect_swapped(G_OBJECT(list), "close",
			G_CALLBACK(gtk_widget_hide), list);
	g_signal_connect(G_OBJECT(list), "delete-event",
			G_CALLBACK(xfsm_session_control_delete), list);
}

/*
 */
static void
xfsm_session_control_finalize(GObject *object)
{
	XfsmSessionControl *list;

	g_return_if_fail(XFSM_IS_SESSION_CONTROL(object));

	list = XFSM_SESSION_CONTROL(object);

	/* free all list items */
	g_list_foreach(list->clients, (GFunc)g_free, NULL);
	g_list_free(list->clients);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/*
 */
GtkWidget *
xfsm_session_control_new(void)
{
	return(GTK_WIDGET(g_object_new(xfsm_session_control_get_type(), NULL)));
}

/*
 */
void
xfsm_session_control_append(XfsmSessionControl *list, Client *client)
{
	GtkTreeModel *store;
	ListItem *item;

	g_return_if_fail(client != NULL);
	g_return_if_fail(list != NULL);

	/* */
	item = g_new(ListItem, 1);
	item->client = client;
	list->clients = g_list_append(list->clients, item);

	/* */
	store = gtk_tree_view_get_model(GTK_TREE_VIEW(list->tree));
	gtk_list_store_append(GTK_LIST_STORE(store), &item->iter);

	/* */
	xfsm_session_control_update(list, client);
}

/*
 */
void
xfsm_session_control_update(XfsmSessionControl *list, Client *client)
{
	GtkTreeModel *store;
	gchar *program;
	gchar *userid;
	gchar **argv;
	GList *lp;

	g_return_if_fail(client != NULL);
	g_return_if_fail(list != NULL);

	/* find client on list */
	for (lp = g_list_first(list->clients); lp != NULL; lp = lp->next)
		if (LIST_ITEM(lp->data)->client == client)
			break;

	/* client is not on the list, this should not happen */
	if (lp == NULL)
		return;

	/* */
	if ((program = client_get_program(client)) == NULL) {
		if ((argv = client_get_restart_command(client)) != NULL) {
			program = g_strjoinv(" ", argv);
			g_strfreev(argv);
		}
		else
			program = g_strdup(_("Unknown"));
	}
	if ((userid = client_get_userid(client)) == NULL)
		userid = g_strdup(_("Unknown"));

	/* */
	store = gtk_tree_view_get_model(GTK_TREE_VIEW(list->tree));
	gtk_list_store_set(GTK_LIST_STORE(store), &LIST_ITEM(lp->data)->iter,
			PROGRAM_COLUMN, program,
			USERID_COLUMN, userid,
			PRIORITY_COLUMN, client_get_priority(client),
			STATE_COLUMN, _(state_names[client_state(client)]),
			-1);

	/* */
	g_free(program);
	g_free(userid);
}

/*
 */
void
xfsm_session_control_remove(XfsmSessionControl *list, Client *client)
{
	GtkTreeModel *store;
	GList *lp;

	g_return_if_fail(client != NULL);
	g_return_if_fail(list != NULL);

	/* find client on list */
	for (lp = g_list_first(list->clients); lp != NULL; lp = lp->next)
		if (LIST_ITEM(lp->data)->client == client)
			break;

	/* client is not on the list, this should not happen */
	if (lp == NULL)
		return;

	/* */
	store = gtk_tree_view_get_model(GTK_TREE_VIEW(list->tree));
	gtk_list_store_remove(GTK_LIST_STORE(store),
			&LIST_ITEM(lp->data)->iter);

	/* */
	g_free(LIST_ITEM(lp->data));
	list->clients = g_list_delete_link(list->clients, lp);
}

