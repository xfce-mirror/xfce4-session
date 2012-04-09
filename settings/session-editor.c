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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <X11/SM/SMlib.h>

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <dbus/dbus-glib.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "xfce4-session-settings-common.h"
#include "xfce4-session-marshal.h"
#include "xfsm-client-dbus-client.h"
#include "xfsm-manager-dbus-client.h"

#define GsmPriority       "_GSM_Priority"
#define GsmDesktopFile    "_GSM_DesktopFile"
#define TREE_ROW_REF_KEY  "--tree-row-ref"

enum
{
    COL_OBJ_PATH = 0,
    COL_NAME,
    COL_ICON_NAME,
    COL_COMMAND,
    COL_RESTART_STYLE,
    COL_RESTART_STYLE_STR,
    COL_PRIORITY,
    COL_PID,
    COL_DBUS_PROXY,
    COL_HAS_DESKTOP_FILE,
    N_COLS,
};

static const gchar *restart_styles[] = {
    N_("If running"),
    N_("Always"),
    N_("Immediately"),
    N_("Never"),
    NULL,
};

static DBusGConnection *dbus_conn = NULL;
static DBusGProxy *manager_dbus_proxy = NULL;


static gboolean
session_editor_ensure_dbus(void)
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

        dbus_g_proxy_add_signal(manager_dbus_proxy, "ClientRegistered",
                                G_TYPE_STRING, G_TYPE_INVALID);
        dbus_g_proxy_add_signal(manager_dbus_proxy, "StateChanged",
                                G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INVALID);
    }

    return !!dbus_conn;
}

static void
manager_state_changed_saving(DBusGProxy *proxy,
                             guint old_state,
                             guint new_state,
                             gpointer user_data)
{
    if(new_state == 1)  /* idle.  FIXME: enum this */
        gtk_dialog_response(GTK_DIALOG(user_data), GTK_RESPONSE_ACCEPT);
}

static gboolean
pulse_session_save_dialog(gpointer data)
{
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(data));
    return TRUE;
}

static void
session_editor_save_session(GtkWidget *btn,
                            GtkWidget *dialog)
{
    GtkWidget *pbar = g_object_get_data(G_OBJECT(dialog), "pbar");
    guint pulse_id;
    GError *error = NULL;

    gtk_widget_set_sensitive(btn, FALSE);

    if(!xfsm_manager_dbus_client_checkpoint(manager_dbus_proxy, "", &error)) {
        xfce_message_dialog(GTK_WINDOW(gtk_widget_get_toplevel(btn)),
                            _("Session Save Error"), GTK_STOCK_DIALOG_ERROR,
                            _("Unable to save the session"),
                            error->message,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                            NULL);
        gtk_widget_set_sensitive(btn, TRUE);
        g_error_free(error);
        return;
    }

    dbus_g_proxy_connect_signal(manager_dbus_proxy, "StateChanged",
                                G_CALLBACK(manager_state_changed_saving),
                                dialog, NULL);
    pulse_id = g_timeout_add(250, pulse_session_save_dialog, pbar);

    gtk_dialog_run(GTK_DIALOG(dialog));

    g_source_remove(pulse_id);
    dbus_g_proxy_disconnect_signal(manager_dbus_proxy, "StateChanged",
                                   G_CALLBACK(manager_state_changed_saving),
                                   dialog);
    gtk_widget_hide(dialog);
    gtk_widget_set_sensitive(btn, TRUE);
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
session_editor_clear_sessions(GtkWidget *btn,
                              GtkWidget *treeview)
{
    gtk_widget_set_sensitive(btn, FALSE);

    if(xfce_message_dialog(GTK_WINDOW(gtk_widget_get_toplevel(treeview)),
                           _("Clear sessions"), GTK_STOCK_DIALOG_QUESTION,
                           _("Are you sure you want to empty the session cache?"),
                           _("The saved states of your applications will not be restored during your next login."),
                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                           XFCE_BUTTON_TYPE_MIXED, GTK_STOCK_OK, _("_Proceed"), GTK_RESPONSE_ACCEPT,
                           NULL) == GTK_RESPONSE_ACCEPT)
    {
        const gchar *item_name;
        gchar       *cache_dir_path, *item_path;
        GDir        *cache_dir;
        GError      *error = NULL;
        gboolean     failed = FALSE;

        cache_dir_path = g_build_path(G_DIR_SEPARATOR_S, g_get_user_cache_dir(), "sessions", NULL);
        cache_dir = g_dir_open(cache_dir_path, 0, &error);
        if(!cache_dir) {
            g_critical("Failed to open the session cache's directory: %s", error->message);
            g_error_free(error);
            g_free(cache_dir_path);
            gtk_widget_set_sensitive(btn, TRUE);
            return;
        }

        while((item_name = g_dir_read_name(cache_dir))) {
            /* only clean Xfce related items */
            if(!g_str_has_prefix(item_name, "xfce4-session-") &&
               !g_str_has_prefix(item_name, "Thunar-") &&
               !g_str_has_prefix(item_name, "xfwm4-")) {
                continue;
            }

            item_path = g_build_filename(cache_dir_path, item_name, NULL);
            if(G_UNLIKELY(g_unlink(item_path) == -1)) {
                DBG("Failed to delete \"%s\" from the session cache.", item_path);
                failed = TRUE;
            }
            g_free(item_path);
        }

        if(failed){
            gchar *secondary_text;

            secondary_text = g_strdup_printf (_("You might need to delete some files manually in \"%s\"."), cache_dir_path);
            xfce_dialog_show_warning(GTK_WINDOW(gtk_widget_get_toplevel(treeview)),
                                     secondary_text,
                                     _("All Xfce cache files could not be cleared"));
            g_free(secondary_text);
        }

        g_dir_close(cache_dir);
        g_free(cache_dir_path);
    }
    else {
        gtk_widget_set_sensitive(btn, TRUE);
    }
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
    gchar *primary;

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
    if(xfce_message_dialog(GTK_WINDOW(gtk_widget_get_toplevel(treeview)),
                           _("Terminate Program"), GTK_STOCK_DIALOG_QUESTION,
                           primary,
                           _("The application will lose any unsaved state and will not be restarted in your next session."),
                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                           XFCE_BUTTON_TYPE_MIXED, GTK_STOCK_QUIT, _("_Quit Program"), GTK_RESPONSE_ACCEPT,
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
    g_free(name);
    g_object_unref(proxy);
}

static void
session_editor_set_from_desktop_file(GtkTreeModel *model,
                                     GtkTreeIter *iter,
                                     const gchar *desktop_file)
{
    XfceRc *rcfile;
    const gchar *name, *icon;

    rcfile = xfce_rc_simple_open(desktop_file, TRUE);
    if(!rcfile)
        return;

    if(!xfce_rc_has_group(rcfile, "Desktop Entry")) {
        xfce_rc_close(rcfile);
        return;
    }

    xfce_rc_set_group(rcfile, "Desktop Entry");

    name = xfce_rc_read_entry(rcfile, "Name", NULL);
    if(!name) {
        /* we require at least Name to make things simpler */
        xfce_rc_close(rcfile);
        return;
    }

    icon = xfce_rc_read_entry(rcfile, "Icon", NULL);

    gtk_list_store_set(GTK_LIST_STORE(model), iter,
                       COL_NAME, name,
                       COL_ICON_NAME, icon,
                       COL_HAS_DESKTOP_FILE, TRUE,
                       -1);

    xfce_rc_close(rcfile);
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
    gboolean has_desktop_file = FALSE;

    if(!gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_path_free(path);
        return;
    }
    gtk_tree_path_free(path);

    gtk_tree_model_get(model, &iter,
                       COL_HAS_DESKTOP_FILE, &has_desktop_file,
                       -1);

    if(!strcmp(name, SmProgram) && G_VALUE_HOLDS_STRING(value)) {
        if(!has_desktop_file) {
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               COL_NAME, g_value_get_string(value),
                               -1);
        }
    } else if(!strcmp(name, SmRestartStyleHint) && G_VALUE_HOLDS_UCHAR(value)) {
        guchar hint = g_value_get_uchar(value);

        if(hint > SmRestartNever)
            hint = SmRestartIfRunning;

        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           COL_RESTART_STYLE, hint,
                           COL_RESTART_STYLE_STR, _(restart_styles[hint]),
                           -1);
    } else if(!strcmp(name, GsmPriority) && G_VALUE_HOLDS_UCHAR(value)) {
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           COL_PRIORITY, g_value_get_uchar(value),
                           -1);
    } else if(!strcmp(name, SmProcessID) && G_VALUE_HOLDS_STRING(value)) {
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           COL_PID, g_value_get_string(value),
                           -1);
    } else if(!strcmp(name, GsmDesktopFile) && G_VALUE_HOLDS_STRING(value)) {
        session_editor_set_from_desktop_file(model, &iter,
                                             g_value_get_string(value));
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
    const gchar *propnames[] = {
        SmProgram, SmRestartStyleHint,SmProcessID, GsmPriority,
        GsmDesktopFile, NULL
    };
    GValue *val;
    const gchar *name = NULL, *pid = NULL;
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
    if((val = g_hash_table_lookup(properties, SmRestartStyleHint))) {
        hint = g_value_get_uchar(val);
        if(hint > SmRestartNever)
            hint = SmRestartIfRunning;
    }
    if((val = g_hash_table_lookup(properties, GsmPriority)))
        priority = g_value_get_uchar(val);
    if((val = g_hash_table_lookup(properties, SmProcessID)))
        pid = g_value_get_string(val);

    if(!name || !*name)
        name = _("(Unknown program)");

    DBG("adding '%s', obj path %s", name, object_path);
    gtk_list_store_append(GTK_LIST_STORE(model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                       COL_DBUS_PROXY, client_proxy,
                       COL_OBJ_PATH, object_path,
                       COL_NAME, name,
                       COL_RESTART_STYLE, hint,
                       COL_RESTART_STYLE_STR, _(restart_styles[hint]),
                       COL_PRIORITY, priority,
                       COL_PID, pid,
                       -1);

    if((val = g_hash_table_lookup(properties, GsmDesktopFile))
       && G_VALUE_HOLDS_STRING(val))
    {
        session_editor_set_from_desktop_file(model, &iter,
                                             g_value_get_string(val));
    }

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

static GtkTreeModel *
session_editor_create_restart_style_combo_model(void)
{
    GtkListStore *ls = gtk_list_store_new(1, G_TYPE_STRING);
    GtkTreeIter iter;
    gint i;

    for(i = 0; restart_styles[i]; ++i) {
        gtk_list_store_append(ls, &iter);
        gtk_list_store_set(ls, &iter, 0, _(restart_styles[i]), -1);
    }

    return GTK_TREE_MODEL(ls);
}

static void
priority_changed(GtkCellRenderer *render,
                 const gchar *path_str,
                 const gchar *new_text,
                 gpointer user_data)
{
    GtkTreeView *treeview = user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    GtkTreeIter iter;

    if(gtk_tree_model_get_iter(model, &iter, path)) {
        DBusGProxy *proxy = NULL;
        gint new_prio_i = atoi(new_text);
        guchar old_prio, new_prio;

        /* FIXME: should probably inform user if value gets clamped */
        if(new_prio_i > G_MAXUINT8)
            new_prio = G_MAXUINT8;
        else if(new_prio_i < 0)
            new_prio = 0;
        else
            new_prio = (guchar)new_prio_i;

        gtk_tree_model_get(model, &iter,
                           COL_DBUS_PROXY, &proxy,
                           COL_PRIORITY, &old_prio,
                           -1);
        if(old_prio != new_prio) {
            GHashTable *properties = g_hash_table_new(g_str_hash, g_str_equal);
            GValue val = { 0, };
            GError *error = NULL;

            g_value_init(&val, G_TYPE_UCHAR);
            g_value_set_uchar(&val, new_prio);
            g_hash_table_insert(properties, GsmPriority, &val);

            if(!xfsm_client_dbus_client_set_sm_properties(proxy, properties, &error)) {
                /* FIXME: show error */
                g_error_free(error);
            }

            g_value_unset(&val);
            g_hash_table_destroy(properties);
        }

        g_object_unref(proxy);
    }

    gtk_tree_path_free(path);
}

static void
restart_style_hint_changed(GtkCellRenderer *render,
                           const gchar *path_str,
                           const gchar *new_text,
                           gpointer user_data)
{
    GtkTreeView *treeview = user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    GtkTreeIter iter;

    if(gtk_tree_model_get_iter(model, &iter, path)) {
        gint i;
        guchar old_hint = SmRestartIfRunning, hint;
        DBusGProxy *proxy = NULL;

        gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
                           COL_DBUS_PROXY, &proxy,
                           COL_RESTART_STYLE, &old_hint,
                           -1);
        hint = old_hint;
        for(i = 0; restart_styles[i]; ++i) {
            if(!g_utf8_collate(new_text, _(restart_styles[i]))) {
                hint = i;
                break;
            }
        }

        if(old_hint != hint) {
            GHashTable *properties = g_hash_table_new(g_str_hash, g_str_equal);
            GValue val = { 0, };
            GError *error = NULL;

            g_value_init(&val, G_TYPE_UCHAR);
            g_value_set_uchar(&val, hint);
            g_hash_table_insert(properties, SmRestartStyleHint, &val);

            if(!xfsm_client_dbus_client_set_sm_properties(proxy, properties, &error)) {
                /* FIXME: show error */
                g_error_free(error);
            }

            g_value_unset(&val);
            g_hash_table_destroy(properties);

            gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                COL_RESTART_STYLE_STR, new_text, -1);
        }

        g_object_unref(proxy);
    }

    gtk_tree_path_free(path);
}

static gint
session_tree_compare_iter(GtkTreeModel *model,
                          GtkTreeIter *a,
                          GtkTreeIter *b,
                          gpointer user_data)
{
    guchar aprio = 0, bprio = 0;

    gtk_tree_model_get(model, a, COL_PRIORITY, &aprio, -1);
    gtk_tree_model_get(model, b, COL_PRIORITY, &bprio, -1);

    if(aprio < bprio)
        return -1;
    else if(aprio > bprio)
        return 1;
    else {
        gint ret;
        gchar *aname = NULL, *bname = NULL;

        gtk_tree_model_get(model, a, COL_NAME, &aname, -1);
        gtk_tree_model_get(model, b, COL_NAME, &bname, -1);

        if(!aname && !bname)
            return 0;
        else if(!aname)
            ret = -1;
        else if(!bname)
            ret = 1;
        else
            ret = g_utf8_collate(aname, bname);

        g_free(aname);
        g_free(bname);

        return ret;
    }
}

static void
session_editor_populate_treeview(GtkTreeView *treeview)
{
    GtkCellRenderer *render;
    GtkTreeViewColumn *col;
    GtkTreeModel *combo_model;
    GPtrArray *clients = NULL;
    GtkListStore *ls;
    guint i;
    GError *error = NULL;

    render = gtk_cell_renderer_text_new();
    g_object_set(render,
                 "editable", TRUE,
                 "editable-set", TRUE,
                 NULL);
    col = gtk_tree_view_column_new_with_attributes(_("Priority"), render,
                                                   "text", COL_PRIORITY,
                                                   NULL);
    gtk_tree_view_append_column(treeview, col);
    g_signal_connect(render, "edited",
                     G_CALLBACK(priority_changed), treeview);

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes(_("PID"), render,
                                                   "text", COL_PID,
                                                   NULL);
    gtk_tree_view_append_column(treeview, col);

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, _("Program"));
    g_object_set(col, "expand", TRUE, NULL);
    gtk_tree_view_append_column(treeview, col);

    render = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(col, render, FALSE);
    gtk_tree_view_column_set_attributes(col, render,
                                        "icon-name", COL_ICON_NAME,
                                        NULL);

    render = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, render, TRUE);
    gtk_tree_view_column_set_attributes(col, render,
                                        "text", COL_NAME,
                                        NULL);

    render = gtk_cell_renderer_combo_new();
    combo_model = session_editor_create_restart_style_combo_model();
    g_object_set(render,
                 "has-entry", FALSE,
                 "model", combo_model,
                 "text-column", 0,
                 "editable", TRUE,
                 "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
                 NULL);
    col = gtk_tree_view_column_new_with_attributes(_("Restart Style"), render,
                                                   "text", COL_RESTART_STYLE_STR,
                                                   NULL);
    gtk_tree_view_append_column(treeview, col);
    g_object_unref(combo_model);
    g_signal_connect(render, "edited",
                     G_CALLBACK(restart_style_hint_changed), treeview);

    if(!session_editor_ensure_dbus())
        return;

    ls = gtk_list_store_new(N_COLS, G_TYPE_STRING, G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UCHAR,
                            G_TYPE_STRING, G_TYPE_UCHAR, G_TYPE_STRING,
                            DBUS_TYPE_G_PROXY, G_TYPE_BOOLEAN);
    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(ls));
    gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(ls),
                                            session_tree_compare_iter,
                                            NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(ls),
                                         GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                         GTK_SORT_ASCENDING);
    g_object_unref(ls);

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
session_editor_init(GtkBuilder *builder)
{
    GObject *btn_save, *btn_clear, *btn_quit, *dlg_saving;
    GtkTreeView *treeview;
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

    treeview = GTK_TREE_VIEW(gtk_builder_get_object(builder, "treeview_clients"));
    sel = gtk_tree_view_get_selection(treeview);
    session_editor_populate_treeview(treeview);

    dlg_saving = gtk_builder_get_object(builder, "dialog_saving");
    g_object_set_data(dlg_saving, "pbar",
                      GTK_WIDGET(gtk_builder_get_object(builder, "progress_save_session")));

    btn_save = gtk_builder_get_object(builder, "btn_save_session");
    g_signal_connect(btn_save, "clicked",
                     G_CALLBACK(session_editor_save_session), GTK_WIDGET(dlg_saving));

    btn_clear = gtk_builder_get_object(builder, "btn_clear_sessions");
    g_signal_connect(btn_clear, "clicked",
                     G_CALLBACK(session_editor_clear_sessions), treeview);

    btn_quit = gtk_builder_get_object(builder, "btn_quit_client");
    g_signal_connect(btn_quit, "clicked",
                     G_CALLBACK(session_editor_quit_client), treeview);
    g_signal_connect(sel, "changed",
                     G_CALLBACK(session_editor_sel_changed_btn), GTK_WIDGET(btn_quit));
}
