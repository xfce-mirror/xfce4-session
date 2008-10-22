/*
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/SM/SMlib.h>

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <dbus/dbus-glib.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "xfce4-session-marshal.h"
#include "xfsm-client-dbus-client.h"
#include "xfsm-manager-dbus-client.h"

#define GsmPriority       "_GSM_Priority"
#define TREE_ROW_REF_KEY  "--tree-row-ref"

enum
{
    COL_OBJ_PATH = 0,
    COL_NAME,
    COL_COMMAND,
    COL_RESTART_STYLE,
    COL_PRIORITY,
    COL_DBUS_PROXY,
    N_COLS,
};

static DBusGConnection *dbus_conn = NULL;
static DBusGProxy *manager_dbus_proxy = NULL;


static gboolean
session_editor_ensure_dbus()
{
    if(G_UNLIKELY(!dbus_conn)) {
        GError *error = NULL;

        dbus_conn = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
        if(!dbus_conn) {
            g_critical("Unable to connect to D-Bus session bus: %s",
                       error ? error->message : "Unknown error");
            if(error)
                g_error_free(error);
        }

        manager_dbus_proxy = dbus_g_proxy_new_for_name(dbus_conn,
                                                       "org.xfce.SessionManager",
                                                       "/org/xfce/SessionManager",
                                                       "org.xfce.Session.Manager");
    }

    return !!dbus_conn;
}

static void
session_editor_sel_changed_btn(GtkTreeSelection *sel,
                               GtkWidget *btn)
{
  GtkTreeIter iter;
  gtk_widget_set_sensitive(btn, gtk_tree_selection_get_selected(sel, NULL,
                                                                &iter));
}

static void
session_editor_remove_client(GtkWidget *btn,
                             GtkWidget *treeview)
{

}

static void
session_editor_quit_client(GtkWidget *btn,
                           GtkWidget *treeview)
{
    GtkTreeSelection *sel;
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    DBusGProxy *proxy = NULL;
    gchar *name = NULL;
    guchar hint = SmRestartIfRunning;
    gchar *primary, *btntext;

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    if(!gtk_tree_selection_get_selected(sel, &model, &iter))
        return;

    gtk_tree_model_get(model, &iter,
                       COL_DBUS_PROXY, &proxy,
                       COL_NAME, &name,
                       COL_RESTART_STYLE, &hint,
                       -1);

    primary = g_strdup_printf(_("Are you sure you want to terminate \"%s\"?"),
                              name);
    btntext = g_strdup_printf(_("Terminate \"%s\""), name);
    if(xfce_message_dialog(GTK_WINDOW(gtk_widget_get_toplevel(treeview)),
                           _("Terminate Program"), GTK_STOCK_DIALOG_QUESTION,
                           primary,
                           _("The application will lose any unsaved state and will not be restarted in your next session."),
                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                           XFCE_CUSTOM_STOCK_BUTTON, btntext, GTK_STOCK_QUIT, GTK_RESPONSE_ACCEPT,
                           NULL) == GTK_RESPONSE_ACCEPT)
    {
        GError *error = NULL;

        if(hint != SmRestartIfRunning) {
            GHashTable *properties = g_hash_table_new(g_str_hash, g_str_equal);
            GValue val = { 0, };

            g_value_init(&val, G_TYPE_UCHAR);
            g_value_set_uchar(&val, SmRestartIfRunning);
            g_hash_table_insert(properties, SmRestartStyleHint, &val);

            if(!xfsm_client_dbus_client_set_sm_properties(proxy, properties, &error)) {
                /* FIXME: show error */
                g_error_free(error);
            }

            g_value_unset(&val);
            g_hash_table_destroy(properties);
        }

        if(!xfsm_client_dbus_client_terminate(proxy, &error)) {
            xfce_message_dialog(GTK_WINDOW(gtk_widget_get_toplevel(treeview)),
                                _("Terminate Program"), GTK_STOCK_DIALOG_ERROR,
                                _("Unable to terminate program."),
                                error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                                NULL);
            g_error_free(error);
        }
    }

    g_free(primary);
    g_free(btntext);
    g_free(name);
    g_object_unref(proxy);
}

static void
client_sm_property_changed(DBusGProxy *proxy,
                           const gchar *name,
                           const GValue *value,
                           gpointer user_data)
{
    GtkTreeView *treeview = user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreeRowReference *rref = g_object_get_data(G_OBJECT(proxy),
                                                  TREE_ROW_REF_KEY);
    GtkTreePath *path = gtk_tree_row_reference_get_path(rref);
    GtkTreeIter iter;

    if(!gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_path_free(path);
        return;
    }
    gtk_tree_path_free(path);

    if(!strcmp(name, SmProgram) && G_VALUE_HOLDS_STRING(value)) {
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           COL_NAME, g_value_get_string(value),
                           -1);
    } else if(!strcmp(name, SmRestartStyleHint) && G_VALUE_HOLDS_UCHAR(value)) {
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           COL_RESTART_STYLE, g_value_get_uchar(value),
                           -1);
    } else if(!strcmp(name, GsmPriority) && G_VALUE_HOLDS_UCHAR(value)) {
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           COL_PRIORITY, g_value_get_uchar(value),
                           -1);
    }
}

static void
client_state_changed(DBusGProxy *proxy,
                     guint old_state,
                     guint new_state,
                     gpointer user_data)
{
    GtkTreeView *treeview = user_data;

    if(new_state == 7) {  /* disconnected.  FIXME: enum this */
        GtkTreeModel *model = gtk_tree_view_get_model(treeview);
        GtkTreeRowReference *rref = g_object_get_data(G_OBJECT(proxy),
                                                      TREE_ROW_REF_KEY);
        GtkTreePath *path;
        GtkTreeIter iter;

        path = gtk_tree_row_reference_get_path(rref);
        if(gtk_tree_model_get_iter(model, &iter, path))
            gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

        gtk_tree_path_free(path);
        g_object_unref(proxy);
    }
}

static void
manager_client_registered(DBusGProxy *proxy,
                          const gchar *object_path,
                          gpointer user_data)
{
    GtkTreeView *treeview = user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreePath *path;
    GtkTreeIter iter;
    DBusGProxy *client_proxy;
    GHashTable *properties = NULL;
    const gchar *propnames[] = { SmProgram, SmRestartStyleHint, GsmPriority, NULL };
    GValue *val;
    const gchar *name = NULL;
    guchar hint = SmRestartIfRunning, priority = 50;
    GError *error = NULL;

    DBG("new client at %s", object_path);

    client_proxy = dbus_g_proxy_new_for_name(dbus_conn,
                                             "org.xfce.SessionManager",
                                             object_path,
                                             "org.xfce.Session.Client");

    if(!xfsm_client_dbus_client_get_sm_properties(client_proxy, propnames,
                                                  &properties, &error))
    {
        g_warning("Unable to get properties for client at %s: %s",
                  object_path, error->message);
        g_clear_error(&error);
        g_object_unref(client_proxy);
        return;
    }

    if((val = g_hash_table_lookup(properties, SmProgram)))
        name = g_value_get_string(val);
    if((val = g_hash_table_lookup(properties, SmRestartStyleHint)))
        hint = g_value_get_uchar(val);
    if((val = g_hash_table_lookup(properties, GsmPriority)))
        priority = g_value_get_uchar(val);

    if(!name || !*name)
        name = _("(Unknown program)");

    DBG("adding '%s', obj path %s", name, object_path);
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                       COL_DBUS_PROXY, client_proxy,
                       COL_OBJ_PATH, object_path,
                       COL_NAME, name,
                       COL_RESTART_STYLE, hint,
                       COL_PRIORITY, priority,
                       -1);

    path = gtk_tree_model_get_path(model, &iter);
    g_object_set_data_full(G_OBJECT(client_proxy), TREE_ROW_REF_KEY,
                           gtk_tree_row_reference_new(model, path),
                           (GDestroyNotify)gtk_tree_row_reference_free);
    gtk_tree_path_free(path);

    dbus_g_proxy_add_signal(client_proxy, "SmPropertyChanged",
                            G_TYPE_STRING, G_TYPE_VALUE,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(client_proxy, "SmPropertyChanged",
                                G_CALLBACK(client_sm_property_changed),
                                treeview, NULL);

    /* proxy will live as long as the client does */
    dbus_g_proxy_add_signal(client_proxy, "StateChanged",
                            G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(client_proxy, "StateChanged",
                                G_CALLBACK(client_state_changed), treeview,
                                NULL);

    g_hash_table_destroy(properties);
}

static void
session_editor_populate_treeview(GtkTreeView *treeview)
{
    GtkCellRenderer *render;
    GtkTreeViewColumn *col;
    GPtrArray *clients = NULL;
    GtkListStore *ls;
    gint i;
    GError *error = NULL;

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes(_("Program"), render,
                                                   "text", COL_NAME,
                                                   NULL);
    gtk_tree_view_append_column(treeview, col);

    if(!session_editor_ensure_dbus())
        return;

    ls = gtk_list_store_new(N_COLS, G_TYPE_STRING, G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_UCHAR, G_TYPE_UCHAR,
                            DBUS_TYPE_G_PROXY);
    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(ls));
    g_object_unref(ls);

    dbus_g_proxy_add_signal(manager_dbus_proxy, "ClientRegistered",
                            G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(manager_dbus_proxy, "ClientRegistered",
                                G_CALLBACK(manager_client_registered),
                                treeview, NULL);

    if(!xfsm_manager_dbus_client_list_clients(manager_dbus_proxy,
                                              &clients, &error))
    {
        g_critical("Unable to query session manager for client list: %s",
                   error->message);
        g_error_free(error);
        return;
    }

    for(i = 0; i < clients->len; ++i) {
        gchar *client_op = g_ptr_array_index(clients, i);
        manager_client_registered(manager_dbus_proxy, client_op, treeview);
        g_free(client_op);
    }

    g_ptr_array_free(clients, TRUE);
}

void
session_editor_init(GladeXML *gxml)
{
    GtkWidget *treeview, *btn_remove, *btn_quit;
    GtkTreeSelection *sel;

    dbus_g_object_register_marshaller(g_cclosure_marshal_VOID__STRING,
                                      G_TYPE_NONE, G_TYPE_STRING,
                                      G_TYPE_INVALID);
    dbus_g_object_register_marshaller(xfce4_session_marshal_VOID__STRING_BOXED,
                                      G_TYPE_NONE, G_TYPE_STRING,
                                      G_TYPE_VALUE, G_TYPE_INVALID);
    dbus_g_object_register_marshaller(xfce4_session_marshal_VOID__UINT_UINT,
                                      G_TYPE_NONE, G_TYPE_UINT, G_TYPE_UINT,
                                      G_TYPE_INVALID);

    treeview = glade_xml_get_widget(gxml, "treeview_clients");
    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    session_editor_populate_treeview(GTK_TREE_VIEW(treeview));

    btn_remove = glade_xml_get_widget(gxml, "btn_remove_client");
    g_signal_connect(btn_remove, "clicked",
                   G_CALLBACK(session_editor_remove_client), treeview);
    //g_signal_connect(sel, "changed",
    //                 G_CALLBACK(session_editor_sel_changed_btn), btn_remove);

    btn_quit = glade_xml_get_widget(gxml, "btn_quit_client");
    g_signal_connect(btn_quit, "clicked",
                   G_CALLBACK(session_editor_quit_client), treeview);
    g_signal_connect(sel, "changed",
                   G_CALLBACK(session_editor_sel_changed_btn), btn_quit);
}
