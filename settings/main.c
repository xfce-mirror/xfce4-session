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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#include <gtk/gtkx.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "libxfsm/xfsm-util.h"

#include "xfae-window.h"
#include "xfce4-session-settings-common.h"
#include "xfce4-session-settings_ui.h"


static void
xfce4_session_settings_dialog_response (GtkDialog *dialog,
                                        gint response,
                                        gpointer userdata)
{
  if (response == GTK_RESPONSE_HELP)
    {
      xfce_dialog_show_help (GTK_WINDOW (dialog), "xfce4-session", "preferences", NULL);
    }
  else
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
      gtk_main_quit ();
    }
}

static void
xfce4_session_settings_show_saved_sessions (GtkBuilder *builder,
                                            GKeyFile *file,
                                            gboolean visible)
{
  GtkWidget *notebook = GTK_WIDGET (gtk_builder_get_object (builder, "plug-child"));
  GtkWidget *sessions_treeview = GTK_WIDGET (gtk_builder_get_object (builder, "saved-sessions-list"));
  GtkTreeModel *model;
  GList *sessions;

  gtk_widget_set_visible (gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 3), visible);
  if (visible == FALSE)
    return;

  settings_list_sessions_treeview_init (GTK_TREE_VIEW (sessions_treeview));
  sessions = settings_list_sessions (file, gtk_widget_get_scale_factor (sessions_treeview));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (sessions_treeview));
  settings_list_sessions_populate (model, sessions);
}

int
main (int argc,
      char **argv)
{
  GtkBuilder *builder;
  GtkWidget *notebook;
  GtkWidget *xfae_page;
  GtkWidget *lbl;
  GtkWidget *label_active_session;
  GObject *delete_button;
  GObject *treeview;
  GError *error = NULL;
  XfconfChannel *channel;
  GKeyFile *file;
  gchar *active_session;
  gchar *active_session_label;
  const gchar *format;
  gchar *markup;

  gulong opt_socket_id = 0;
  gboolean opt_version = FALSE;

  GOptionEntry option_entries[] = {
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_ ("Settings manager socket"), N_ ("SOCKET ID") },
    { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_ ("Version information"), NULL },
    { NULL }
  };

  xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

  if (!gtk_init_with_args (&argc, &argv, NULL, option_entries,
                           GETTEXT_PACKAGE, &error))
    {
      if (G_LIKELY (error))
        {
          g_printerr ("%s: %s.\n", G_LOG_DOMAIN, error->message);
          g_printerr (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
          g_printerr ("\n");
          g_error_free (error);
        }
      else
        g_critical ("Unable to open display.");

      return EXIT_FAILURE;
    }

  if (G_UNLIKELY (opt_version))
    {
      g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
      g_print ("%s\n", "Copyright (c) 2004-2024");
      g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");

      return EXIT_SUCCESS;
    }

  if (G_UNLIKELY (!xfconf_init (&error)))
    {
      xfce_dialog_show_error (NULL, error, _("Unable to contact settings server"));
      g_error_free (error);
      return EXIT_FAILURE;
    }

  gtk_window_set_default_icon_name ("xfce4-session");

  /* hook to make sure the libxfce4ui library is linked */
  if (xfce_titled_dialog_get_type () == 0)
    return EXIT_FAILURE;

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder,
                               xfce4_session_settings_ui,
                               xfce4_session_settings_ui_length,
                               &error);

  if (!builder)
    {
      if (error)
        {
          xfce_dialog_show_error (NULL, error, _("Unable to create user interface from embedded definition data"));
          g_error_free (error);
        }
      return EXIT_FAILURE;
    }

  session_editor_init (builder);

  channel = xfconf_channel_get (SETTINGS_CHANNEL);

  /* FIXME: someday, glade-ify this, maybe. */
  xfae_page = xfae_window_new ();
  gtk_widget_show (xfae_page);
  notebook = GTK_WIDGET (gtk_builder_get_object (builder, "plug-child"));
  lbl = gtk_label_new_with_mnemonic (_("App_lication Autostart"));
  gtk_widget_show (lbl);
  gtk_notebook_insert_page (GTK_NOTEBOOK (notebook), xfae_page, lbl, 1);

  label_active_session = GTK_WIDGET (gtk_builder_get_object (builder, "label_active_session"));
  active_session = xfconf_channel_get_string (channel, "/general/SessionName", "Default");
  active_session_label = _("Currently active session:");
  format = "%s <b>%s</b>";
  markup = g_markup_printf_escaped (format, active_session_label, active_session);
  gtk_label_set_markup (GTK_LABEL (label_active_session), markup);
  g_free (markup);
  g_free (active_session);

  delete_button = gtk_builder_get_object (builder, "btn_delete_session");
  treeview = gtk_builder_get_object (builder, "saved-sessions-list");
  g_signal_connect (delete_button, "clicked", G_CALLBACK (settings_list_sessions_delete_session), GTK_TREE_VIEW (treeview));

  if (!WINDOWING_IS_X11 ())
    {
      gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "frame1")));
      gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "chk_session_autosave")));
      gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "vbox8")));
      gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "frame4")));
    }

  /* Check if there are saved sessions and if so, show the "Saved Sessions" tab */
  file = settings_list_sessions_open_key_file (TRUE);
  xfce4_session_settings_show_saved_sessions (builder, file, file != NULL && WINDOWING_IS_X11 ());
  if (file != NULL)
    g_key_file_free (file);

  /* bind widgets to xfconf */
  xfconf_g_property_bind (channel, "/chooser/AlwaysDisplay", G_TYPE_BOOLEAN,
                          gtk_builder_get_object (builder, "chk_display_chooser"),
                          "active");
  xfconf_g_property_bind (channel, "/general/SaveOnExit", G_TYPE_BOOLEAN,
                          gtk_builder_get_object (builder, "chk_session_autosave"),
                          "active");
  xfconf_g_property_bind (channel, "/general/PromptOnLogout", G_TYPE_BOOLEAN,
                          gtk_builder_get_object (builder, "chk_logout_prompt"),
                          "active");
  xfconf_g_property_bind (channel, "/compat/LaunchGNOME", G_TYPE_BOOLEAN,
                          gtk_builder_get_object (builder, "chk_compat_gnome"),
                          "active");
  xfconf_g_property_bind (channel, "/compat/LaunchKDE", G_TYPE_BOOLEAN,
                          gtk_builder_get_object (builder, "chk_compat_kde"),
                          "active");
  xfconf_g_property_bind (channel, "/security/EnableTcp", G_TYPE_BOOLEAN,
                          gtk_builder_get_object (builder, "chk_enable_tcp"),
                          "active");
  xfconf_g_property_bind (channel, "/shutdown/LockScreen", G_TYPE_BOOLEAN,
                          gtk_builder_get_object (builder, "chk_lock_screen"),
                          "active");

  if (opt_socket_id == 0 || !WINDOWING_IS_X11 ())
    {
      GtkWidget *dialog = GTK_WIDGET (gtk_builder_get_object (builder, "xfce4_session_settings_dialog"));

      g_signal_connect (dialog, "response", G_CALLBACK (xfce4_session_settings_dialog_response), NULL);
      g_signal_connect (dialog, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

      gtk_widget_show (dialog);

#ifdef ENABLE_X11
      if (WINDOWING_IS_X11 ())
        {
          /* To prevent the settings dialog to be saved in the session */
          gdk_x11_set_sm_client_id ("FAKE ID");
        }
#endif

      gtk_main ();
    }
  else
    {
#ifdef ENABLE_X11
      GtkWidget *plug;
      GtkWidget *plug_child;
      GtkWidget *parent;

      plug_child = GTK_WIDGET (gtk_builder_get_object (builder, "plug-child"));
      plug = gtk_plug_new (opt_socket_id);
      gtk_widget_show (plug);

      parent = gtk_widget_get_parent (plug_child);
      if (parent)
        {
          g_object_ref (plug_child);
          gtk_container_remove (GTK_CONTAINER (parent), plug_child);
          gtk_container_add (GTK_CONTAINER (plug), plug_child);
          g_object_unref (plug_child);
        }
      g_signal_connect (plug, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

      /* Stop startup notification */
      gdk_notify_startup_complete ();

      gtk_main ();
#endif
    }

  g_object_unref (builder);
  xfconf_shutdown ();

  return EXIT_SUCCESS;
}
