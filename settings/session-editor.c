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
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <X11/SM/SMlib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#include "libxfsm/xfsm-util.h"

#include "xfce4-session-marshal.h"
#include "xfce4-session-settings-common.h"
#include "xfsm-client-dbus-client.h"
#include "xfsm-manager-dbus-client.h"

#define GsmPriority "_GSM_Priority"
#define GsmDesktopFile "_GSM_DesktopFile"
#define TREE_ROW_REF_KEY "--tree-row-ref"

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
  N_ ("If running"),
  N_ ("Always"),
  N_ ("Immediately"),
  N_ ("Never"),
  NULL,
};

static XfsmManager *manager_dbus_proxy = NULL;


static gboolean
session_editor_ensure_dbus (void)
{
  GError *error = NULL;

  TRACE ("entering\n");

  if (manager_dbus_proxy)
    return TRUE;

  manager_dbus_proxy = xfsm_manager_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                            G_DBUS_PROXY_FLAGS_NONE,
                                                            "org.xfce.SessionManager",
                                                            "/org/xfce/SessionManager",
                                                            NULL,
                                                            &error);

  if (manager_dbus_proxy == NULL)
    {
      g_critical ("error connecting to org.xfce.SessionManager, reason was: %s", error->message);
      g_clear_error (&error);
      return FALSE;
    }

  return TRUE;
}

static void
manager_state_changed_saving (XfsmManager *proxy,
                              guint old_state,
                              guint new_state,
                              gpointer user_data)
{
  if (new_state == 1) /* idle.  FIXME: enum this */
    gtk_dialog_response (GTK_DIALOG (user_data), GTK_RESPONSE_ACCEPT);
}

static gboolean
pulse_session_save_dialog (gpointer data)
{
  gtk_progress_bar_pulse (GTK_PROGRESS_BAR (data));
  return TRUE;
}

static void
session_editor_save_session (GtkWidget *btn,
                             GtkBuilder *builder)
{
  GtkWidget *dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_saving"));
  GtkWidget *treeview = GTK_WIDGET (gtk_builder_get_object (builder, "saved-sessions-list"));
  GtkWidget *notebook = GTK_WIDGET (gtk_builder_get_object (builder, "plug-child"));
  GtkWidget *btn_clear = GTK_WIDGET (gtk_builder_get_object (builder, "btn_clear_sessions"));
  GtkWidget *pbar = g_object_get_data (G_OBJECT (dialog), "pbar");
  GtkTreeModel *model;
  GList *sessions;
  GKeyFile *file;
  guint pulse_id;
  guint sig_id;
  GError *error = NULL;

  TRACE ("entering\n");

  gtk_widget_set_sensitive (btn, FALSE);

  if (!xfsm_manager_call_checkpoint_sync (manager_dbus_proxy, "", NULL, &error))
    {
      xfce_message_dialog (GTK_WINDOW (gtk_widget_get_toplevel (btn)),
                           _("Session Save Error"), "dialog-error", _("Unable to save the session"),
                           error ? error->message : "Unknown error.",
                           XFCE_BUTTON_TYPE_MIXED, "window-close-symbolic", _("_Close"), GTK_RESPONSE_ACCEPT,
                           NULL);
      gtk_widget_set_sensitive (btn, TRUE);
      if (error)
        g_error_free (error);
      return;
    }

  sig_id = g_signal_connect (manager_dbus_proxy, "state_changed",
                             G_CALLBACK (manager_state_changed_saving),
                             dialog);
  pulse_id = g_timeout_add (250, pulse_session_save_dialog, pbar);

  gtk_dialog_run (GTK_DIALOG (dialog));

  g_source_remove (pulse_id);
  g_signal_handler_disconnect (manager_dbus_proxy, sig_id);
  gtk_widget_hide (dialog);
  gtk_widget_set_sensitive (btn, TRUE);

  /* After saving the session we ensure the clear button is sensitive */
  gtk_widget_set_sensitive (btn_clear, TRUE);

  /* Always make sure the "Saved Sessions" tab is visible  and the treeview is populated after saving a session */
  file = settings_list_sessions_open_key_file (TRUE);
  if (file != NULL)
    {
      gtk_widget_show (gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 3));
      sessions = settings_list_sessions (file, gtk_widget_get_scale_factor (treeview));
      model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

      /* If the treeview hasn't been initialized we do it now */
      if (!GTK_IS_LIST_STORE (model))
        {
          settings_list_sessions_treeview_init (GTK_TREE_VIEW (treeview));
          model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
        }
      settings_list_sessions_populate (model, sessions);
      g_key_file_free (file);
    }
}

static void
session_editor_sel_changed_btn (GtkTreeSelection *sel,
                                GtkWidget *btn)
{
  GtkTreeIter iter;
  gtk_widget_set_sensitive (btn, gtk_tree_selection_get_selected (sel, NULL, &iter));
}

static void
session_editor_clear_sessions (GtkWidget *btn,
                               GtkBuilder *builder)
{
  GtkWidget *treeview = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_clients"));
  GtkWidget *notebook = GTK_WIDGET (gtk_builder_get_object (builder, "plug-child"));
  const gchar *text = _("The saved states of your applications will not be restored during your next login.");
  TRACE ("entering\n");

  gtk_widget_set_sensitive (btn, FALSE);

  if (xfce_message_dialog (GTK_WINDOW (gtk_widget_get_toplevel (treeview)), _("Clear sessions"),
                           "dialog-question", _("Are you sure you want to empty the session cache?"),
                           text, _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Proceed"), GTK_RESPONSE_ACCEPT,
                           NULL)
      == GTK_RESPONSE_ACCEPT)
    {
      const gchar *item_name;
      gchar *cache_dir_path, *item_path;
      GDir *cache_dir;
      GError *error = NULL;
      gboolean failed = FALSE;

      cache_dir_path = g_build_path (G_DIR_SEPARATOR_S, g_get_user_cache_dir (), "sessions", NULL);
      cache_dir = g_dir_open (cache_dir_path, 0, &error);
      if (!cache_dir)
        {
          g_critical ("Failed to open the session cache's directory: %s", error->message);
          g_error_free (error);
          g_free (cache_dir_path);
          gtk_widget_set_sensitive (btn, TRUE);
          return;
        }

      while ((item_name = g_dir_read_name (cache_dir)))
        {
          /* only clean Xfce related items */
          if (!g_str_has_prefix (item_name, "xfce4-session-")
              && !g_str_has_prefix (item_name, "Thunar-")
              && !g_str_has_prefix (item_name, "xfwm4-"))
            {
              continue;
            }

          item_path = g_build_filename (cache_dir_path, item_name, NULL);
          if (G_UNLIKELY (g_unlink (item_path) == -1))
            {
              DBG ("Failed to delete \"%s\" from the session cache.", item_path);
              failed = TRUE;
            }
          g_free (item_path);
        }

      if (failed)
        {
          gchar *secondary_text;

          secondary_text = g_strdup_printf (_("You might need to delete some files manually in \"%s\"."), cache_dir_path);
          xfce_dialog_show_warning (GTK_WINDOW (gtk_widget_get_toplevel (treeview)),
                                    secondary_text,
                                    _("All Xfce cache files could not be cleared"));
          g_free (secondary_text);
        }

      g_dir_close (cache_dir);
      g_free (cache_dir_path);

      /* Always make sure the "Saved Sessions" tab is hidden after deleting all sessions */
      gtk_widget_hide (gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 3));
    }
  else
    {
      gtk_widget_set_sensitive (btn, TRUE);
    }
}

static void
session_editor_quit_client (GtkWidget *btn,
                            GtkWidget *treeview)
{
  GtkTreeSelection *sel;
  GtkTreeModel *model = NULL;
  GtkTreeIter iter;
  XfsmClient *proxy = NULL;
  gchar *name = NULL;
  guchar hint = SmRestartIfRunning;
  gchar *primary;
  const gchar *text;

  TRACE ("entering\n");

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  if (!gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      COL_DBUS_PROXY, &proxy,
                      COL_NAME, &name,
                      COL_RESTART_STYLE, &hint,
                      -1);

  primary = g_strdup_printf (_("Are you sure you want to terminate \"%s\"?"), name);
  text = _("The application will lose any unsaved state and will not be restarted in your next session.");
  if (xfce_message_dialog (GTK_WINDOW (gtk_widget_get_toplevel (treeview)),
                           _("Terminate Program"), "dialog-question",
                           primary, text, _("_Cancel"), GTK_RESPONSE_CANCEL, XFCE_BUTTON_TYPE_MIXED,
                           "application-exit", _("_Quit Program"), GTK_RESPONSE_ACCEPT,
                           NULL)
      == GTK_RESPONSE_ACCEPT)
    {
      GError *error = NULL;

      if (hint != SmRestartIfRunning)
        {
          GVariantBuilder properties;
          GVariant *variant;

          g_variant_builder_init (&properties, G_VARIANT_TYPE ("a{sv}"));
          variant = g_variant_new_byte (SmRestartIfRunning);
          g_variant_builder_add (&properties, "{sv}", SmRestartStyleHint, variant);

          if (!xfsm_client_call_set_sm_properties_sync (proxy, g_variant_builder_end (&properties), NULL, &error))
            {
              g_critical ("error setting 'SmRestartStyleHint', error: %s", error->message);
              g_clear_error (&error);
            }
        }

      if (!xfsm_client_call_terminate_sync (proxy, NULL, &error))
        {
          xfce_message_dialog (GTK_WINDOW (gtk_widget_get_toplevel (treeview)),
                               _("Terminate Program"), "dialog-error", _("Unable to terminate program."),
                               error->message,
                               XFCE_BUTTON_TYPE_MIXED, "window-close-symbolic", _("_Close"), GTK_RESPONSE_ACCEPT,
                               NULL);
          g_error_free (error);
        }
    }

  g_free (primary);
  g_free (name);
  g_object_unref (proxy);
}

static void
session_editor_set_from_desktop_file (GtkTreeModel *model,
                                      GtkTreeIter *iter,
                                      const gchar *desktop_file)
{
  XfceRc *rcfile;
  const gchar *name, *icon;
  GIcon *gicon;

  TRACE ("entering\n");

  rcfile = xfce_rc_simple_open (desktop_file, TRUE);
  if (!rcfile)
    return;

  if (!xfce_rc_has_group (rcfile, "Desktop Entry"))
    {
      xfce_rc_close (rcfile);
      return;
    }

  xfce_rc_set_group (rcfile, "Desktop Entry");

  name = xfce_rc_read_entry (rcfile, "Name", NULL);
  if (!name)
    {
      /* we require at least Name to make things simpler */
      xfce_rc_close (rcfile);
      return;
    }

  icon = xfce_rc_read_entry (rcfile, "Icon", NULL);
  gicon = g_themed_icon_new_with_default_fallbacks (icon);

  gtk_list_store_set (GTK_LIST_STORE (model), iter,
                      COL_NAME, name,
                      COL_ICON_NAME, gicon,
                      COL_HAS_DESKTOP_FILE, TRUE,
                      -1);
  if (gicon != NULL)
    g_object_unref (gicon);

  xfce_rc_close (rcfile);
}

static void
client_sm_property_changed (XfsmClient *proxy,
                            const gchar *name,
                            const GValue *value,
                            gpointer user_data)
{
  GtkTreeView *treeview = user_data;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  GtkTreeRowReference *rref = g_object_get_data (G_OBJECT (proxy),
                                                 TREE_ROW_REF_KEY);
  GtkTreePath *path = gtk_tree_row_reference_get_path (rref);
  GtkTreeIter iter;
  gboolean has_desktop_file = FALSE;

  TRACE ("entering\n");

  if (!gtk_tree_model_get_iter (model, &iter, path))
    {
      gtk_tree_path_free (path);
      return;
    }
  gtk_tree_path_free (path);

  gtk_tree_model_get (model, &iter,
                      COL_HAS_DESKTOP_FILE, &has_desktop_file,
                      -1);

  if (!strcmp (name, SmProgram) && G_VALUE_HOLDS_STRING (value))
    {
      if (!has_desktop_file)
        {
          gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                              COL_NAME, g_value_get_string (value),
                              -1);
        }
    }
  else if (!strcmp (name, SmRestartStyleHint) && G_VALUE_HOLDS_UCHAR (value))
    {
      guchar hint = g_value_get_uchar (value);

      if (hint > SmRestartNever)
        hint = SmRestartIfRunning;

      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          COL_RESTART_STYLE, hint,
                          COL_RESTART_STYLE_STR, _(restart_styles[hint]),
                          -1);
    }
  else if (!strcmp (name, GsmPriority) && G_VALUE_HOLDS_UCHAR (value))
    {
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          COL_PRIORITY, g_value_get_uchar (value),
                          -1);
    }
  else if (!strcmp (name, SmProcessID) && G_VALUE_HOLDS_STRING (value))
    {
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          COL_PID, g_value_get_string (value),
                          -1);
    }
  else if (!strcmp (name, GsmDesktopFile) && G_VALUE_HOLDS_STRING (value))
    {
      session_editor_set_from_desktop_file (model, &iter, g_value_get_string (value));
    }
}

static void
client_state_changed (XfsmClient *proxy,
                      guint old_state,
                      guint new_state,
                      gpointer user_data)
{
  GtkTreeView *treeview = user_data;

  TRACE ("entering\n");

  /* disconnected.  FIXME: enum this */
  if (new_state == 7)
    {
      GtkTreeModel *model = gtk_tree_view_get_model (treeview);
      GtkTreeRowReference *rref = g_object_get_data (G_OBJECT (proxy), TREE_ROW_REF_KEY);
      GtkTreePath *path;
      GtkTreeIter iter;

      path = gtk_tree_row_reference_get_path (rref);
      if (gtk_tree_model_get_iter (model, &iter, path))
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

      gtk_tree_path_free (path);
      g_object_unref (proxy);
    }
}

static void
manager_client_registered (XfsmManager *proxy,
                           const gchar *object_path,
                           gpointer user_data)
{
  GtkTreeView *treeview = user_data;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  GtkTreePath *path;
  GtkTreeIter iter;
  XfsmClient *client_proxy;
  const gchar *propnames[] = {
    SmProgram, SmRestartStyleHint, SmProcessID, GsmPriority,
    GsmDesktopFile, NULL
  };
  const gchar *name = NULL, *pid = NULL, *desktop_file = NULL;
  guchar hint = SmRestartIfRunning, priority = 50;
  GVariant *variant, *variant_value;
  GVariantIter *variant_iter;
  gchar *property;
  GIcon *gicon;
  GError *error = NULL;

  TRACE ("entering\n");

  DBG ("new client at %s", object_path);

  client_proxy = xfsm_client_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                     G_DBUS_PROXY_FLAGS_NONE,
                                                     "org.xfce.SessionManager",
                                                     object_path,
                                                     NULL,
                                                     &error);

  if (error != NULL)
    {
      g_warning ("Unable to connect to org.xfce.SessionManager, reason: %s", error->message);
      g_clear_error (&error);
      return;
    }

  if (!xfsm_client_call_get_sm_properties_sync (client_proxy, propnames,
                                                &variant, NULL, &error))
    {
      g_warning ("Unable to get properties for client at %s: %s",
                 object_path, error->message);
      g_clear_error (&error);
      g_object_unref (client_proxy);
      return;
    }

  g_variant_get ((GVariant *) variant, "a{sv}", &variant_iter);
  while (g_variant_iter_next (variant_iter, "{sv}", &property, &variant_value))
    {
      if (g_strcmp0 (property, SmProgram) == 0)
        {
          name = g_variant_get_string (variant_value, 0);
          DBG ("name %s", name);
        }
      else if (g_strcmp0 (property, SmRestartStyleHint) == 0)
        {
          hint = g_variant_get_byte (variant_value);
          if (hint > SmRestartNever)
            {
              hint = SmRestartIfRunning;
            }
          DBG ("hint %d", hint);
        }
      else if (g_strcmp0 (property, GsmPriority) == 0)
        {
          priority = g_variant_get_byte (variant_value);
          DBG ("priority %d", priority);
        }
      else if (g_strcmp0 (property, SmProcessID) == 0)
        {
          pid = g_variant_get_string (variant_value, 0);
          DBG ("pid %s", pid);
        }
      else if (g_strcmp0 (property, GsmDesktopFile) == 0)
        {
          desktop_file = g_variant_get_string (variant_value, 0);
          DBG ("desktop_file %s", desktop_file);
        }

      g_free (property);
      g_variant_unref (variant_value);
    }
  g_variant_iter_free (variant_iter);

  if (!name || !*name)
    name = _("(Unknown program)");

  DBG ("adding '%s', obj path %s", name, object_path);
  gtk_list_store_append (GTK_LIST_STORE (model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                      COL_DBUS_PROXY, client_proxy,
                      COL_OBJ_PATH, object_path,
                      COL_NAME, name,
                      COL_RESTART_STYLE, hint,
                      COL_RESTART_STYLE_STR, _(restart_styles[hint]),
                      COL_PRIORITY, priority,
                      COL_PID, pid,
                      -1);

  if (desktop_file != NULL)
    {
      session_editor_set_from_desktop_file (model, &iter, desktop_file);
    }
  else
    {
      gicon = xfce_gicon_from_name (name);

      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          COL_ICON_NAME, gicon,
                          -1);
      if (gicon != NULL)
        g_object_unref (gicon);
    }

  path = gtk_tree_model_get_path (model, &iter);
  g_object_set_data_full (G_OBJECT (client_proxy), TREE_ROW_REF_KEY,
                          gtk_tree_row_reference_new (model, path),
                          (GDestroyNotify) gtk_tree_row_reference_free);
  gtk_tree_path_free (path);

  g_signal_connect (client_proxy, "sm_property_changed",
                    G_CALLBACK (client_sm_property_changed),
                    treeview);

  /* proxy will live as long as the client does */
  g_signal_connect (client_proxy, "state_changed",
                    G_CALLBACK (client_state_changed), treeview);

  g_variant_unref (variant);
}

static GtkTreeModel *
session_editor_create_restart_style_combo_model (void)
{
  GtkListStore *ls = gtk_list_store_new (1, G_TYPE_STRING);
  GtkTreeIter iter;
  gint i;

  TRACE ("entering\n");

  for (i = 0; restart_styles[i]; ++i)
    {
      gtk_list_store_append (ls, &iter);
      gtk_list_store_set (ls, &iter, 0, _(restart_styles[i]), -1);
    }

  return GTK_TREE_MODEL (ls);
}

static void
priority_changed (GtkCellRenderer *render,
                  const gchar *path_str,
                  const gchar *new_text,
                  gpointer user_data)
{
  GtkTreeView *treeview = user_data;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  GtkTreeIter iter;

  TRACE ("entering\n");

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      XfsmClient *proxy = NULL;
      gint new_prio_i = atoi (new_text);
      guchar old_prio, new_prio;

      /* FIXME: should probably inform user if value gets clamped */
      if (new_prio_i > G_MAXUINT8)
        new_prio = G_MAXUINT8;
      else if (new_prio_i < 0)
        new_prio = 0;
      else
        new_prio = (guchar) new_prio_i;

      gtk_tree_model_get (model, &iter,
                          COL_DBUS_PROXY, &proxy,
                          COL_PRIORITY, &old_prio,
                          -1);

      DBG ("old_prio %d, new_prio %d", old_prio, new_prio);

      if (old_prio != new_prio)
        {
          GVariantBuilder properties;
          GVariant *variant;
          GError *error = NULL;

          g_variant_builder_init (&properties, G_VARIANT_TYPE ("a{sv}"));
          variant = g_variant_new_byte (new_prio);
          g_variant_builder_add (&properties, "{sv}", GsmPriority, variant);

          if (!xfsm_client_call_set_sm_properties_sync (proxy, g_variant_builder_end (&properties), NULL, &error))
            {
              g_critical ("error setting 'GsmPriority', error: %s", error->message);
              g_clear_error (&error);
            }

          gtk_list_store_set (GTK_LIST_STORE (model), &iter, COL_PRIORITY, new_prio, -1);
        }

      g_object_unref (proxy);
    }

  gtk_tree_path_free (path);
}

static void
restart_style_hint_changed (GtkCellRenderer *render,
                            const gchar *path_str,
                            const gchar *new_text,
                            gpointer user_data)
{
  GtkTreeView *treeview = user_data;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  GtkTreeIter iter;

  TRACE ("entering\n");

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      gint i;
      guchar old_hint = SmRestartIfRunning, hint;
      XfsmClient *proxy = NULL;

      gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
                          COL_DBUS_PROXY, &proxy,
                          COL_RESTART_STYLE, &old_hint,
                          -1);
      hint = old_hint;
      for (i = 0; restart_styles[i]; ++i)
        {
          if (!g_utf8_collate (new_text, _(restart_styles[i])))
            {
              hint = i;
              break;
            }
        }

      DBG ("old_hint %d, hint %d", old_hint, hint);

      if (old_hint != hint)
        {
          GVariantBuilder properties;
          GVariant *variant;
          GError *error = NULL;

          g_variant_builder_init (&properties, G_VARIANT_TYPE ("a{sv}"));
          variant = g_variant_new_byte (hint);
          g_variant_builder_add (&properties, "{sv}", SmRestartStyleHint, variant);

          if (!xfsm_client_call_set_sm_properties_sync (proxy, g_variant_builder_end (&properties), NULL, &error))
            {
              g_critical ("error setting 'SmRestartStyleHint', error: %s", error->message);
              g_clear_error (&error);
            }

          gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                              COL_RESTART_STYLE_STR, new_text, -1);
        }

      g_object_unref (proxy);
    }

  gtk_tree_path_free (path);
}

static gint
session_tree_compare_iter (GtkTreeModel *model,
                           GtkTreeIter *a,
                           GtkTreeIter *b,
                           gpointer user_data)
{
  guchar aprio = 0, bprio = 0;

  TRACE ("entering\n");

  gtk_tree_model_get (model, a, COL_PRIORITY, &aprio, -1);
  gtk_tree_model_get (model, b, COL_PRIORITY, &bprio, -1);

  if (aprio < bprio)
    return -1;
  else if (aprio > bprio)
    return 1;
  else
    {
      gint ret;
      gchar *aname = NULL, *bname = NULL;

      gtk_tree_model_get (model, a, COL_NAME, &aname, -1);
      gtk_tree_model_get (model, b, COL_NAME, &bname, -1);

      if (!aname && !bname)
        return 0;
      else if (!aname)
        ret = -1;
      else if (!bname)
        ret = 1;
      else
        ret = g_utf8_collate (aname, bname);

      g_free (aname);
      g_free (bname);

      return ret;
    }
}

/**
 * If there are sortable and `extend`able columns, the extendable colums
 *  extend too much. Calling `gtk_tree_view_columns_autosize` upon TreeView-
 *  realization fixes the problem
 **/
static void
session_editor_correct_treeview_column_size (GtkWidget *treeview,
                                             gpointer user_data)
{
  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (treeview));
}

static void
session_editor_populate_treeview (GtkTreeView *treeview)
{
  GtkCellRenderer *render;
  GtkTreeViewColumn *col;
  GtkTreeModel *combo_model;
  gchar **clients = NULL;
  GtkListStore *ls;
  guint i;
  GError *error = NULL;

  TRACE ("entering\n");

  // fix buggy sizing behavior of gtk
  g_signal_connect (treeview, "realize", G_CALLBACK (session_editor_correct_treeview_column_size), NULL);

  render = gtk_cell_renderer_text_new ();
  g_object_set (render,
                "editable", TRUE,
                "editable-set", TRUE,
                NULL);
  col = gtk_tree_view_column_new_with_attributes (_("Priority"), render,
                                                  "text", COL_PRIORITY,
                                                  NULL);
  gtk_tree_view_append_column (treeview, col);
  gtk_tree_view_column_set_sort_column_id (col, COL_PRIORITY);

  g_signal_connect (render, "edited",
                    G_CALLBACK (priority_changed), treeview);

  render = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes (_("PID"), render,
                                                  "text", COL_PID,
                                                  NULL);
  gtk_tree_view_append_column (treeview, col);
  gtk_tree_view_column_set_sort_column_id (col, COL_PID);

  col = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (col, _("Program"));
  g_object_set (col, "expand", TRUE, NULL);
  gtk_tree_view_append_column (treeview, col);
  gtk_tree_view_column_set_sort_column_id (col, COL_NAME);

  render = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (col, render, FALSE);
  gtk_tree_view_column_set_attributes (col, render,
                                       "gicon", COL_ICON_NAME,
                                       NULL);

  render = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (col, render, TRUE);
  gtk_tree_view_column_set_attributes (col, render,
                                       "text", COL_NAME,
                                       NULL);

  render = gtk_cell_renderer_combo_new ();
  combo_model = session_editor_create_restart_style_combo_model ();
  g_object_set (render,
                "has-entry", FALSE,
                "model", combo_model,
                "text-column", 0,
                "editable", TRUE,
                "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
                NULL);
  col = gtk_tree_view_column_new_with_attributes (_("Restart Style"), render,
                                                  "text", COL_RESTART_STYLE_STR,
                                                  NULL);
  gtk_tree_view_append_column (treeview, col);
  gtk_tree_view_column_set_sort_column_id (col, COL_RESTART_STYLE_STR);

  g_object_unref (combo_model);
  g_signal_connect (render, "edited",
                    G_CALLBACK (restart_style_hint_changed), treeview);

  if (!session_editor_ensure_dbus ())
    return;

  ls = gtk_list_store_new (N_COLS, G_TYPE_STRING, G_TYPE_STRING,
                           G_TYPE_ICON, G_TYPE_STRING, G_TYPE_UCHAR,
                           G_TYPE_STRING, G_TYPE_UCHAR, G_TYPE_STRING,
                           G_TYPE_OBJECT, G_TYPE_BOOLEAN);
  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (ls));
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (ls),
                                           session_tree_compare_iter,
                                           NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (ls),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING);
  g_object_unref (ls);

  g_signal_connect (manager_dbus_proxy, "client_registered",
                    G_CALLBACK (manager_client_registered),
                    treeview);

  if (!xfsm_manager_call_list_clients_sync (manager_dbus_proxy, &clients, NULL, &error))
    {
      g_critical ("Unable to query session manager for client list: %s", error->message);
      g_error_free (error);
      return;
    }

  for (i = 0; clients[i] != NULL; ++i)
    {
      gchar *client_op = clients[i];
      manager_client_registered (manager_dbus_proxy, client_op, treeview);
    }

  g_strfreev (clients);
}

void
session_editor_init (GtkBuilder *builder)
{
  GObject *btn_save, *btn_clear, *btn_quit, *dlg_saving;
  GtkTreeView *treeview;
  GtkTreeSelection *sel;

  TRACE ("entering\n");

  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder, "treeview_clients"));
  sel = gtk_tree_view_get_selection (treeview);
  session_editor_populate_treeview (treeview);

  dlg_saving = gtk_builder_get_object (builder, "dialog_saving");
  g_object_set_data (dlg_saving, "pbar",
                     GTK_WIDGET (gtk_builder_get_object (builder, "progress_save_session")));

  btn_save = gtk_builder_get_object (builder, "btn_save_session");
  g_signal_connect (btn_save, "clicked",
                    G_CALLBACK (session_editor_save_session), builder);

  btn_clear = gtk_builder_get_object (builder, "btn_clear_sessions");
  g_signal_connect (btn_clear, "clicked",
                    G_CALLBACK (session_editor_clear_sessions), builder);

  btn_quit = gtk_builder_get_object (builder, "btn_quit_client");
  g_signal_connect (btn_quit, "clicked",
                    G_CALLBACK (session_editor_quit_client), treeview);
  g_signal_connect (sel, "changed",
                    G_CALLBACK (session_editor_sel_changed_btn), GTK_WIDGET (btn_quit));
}
