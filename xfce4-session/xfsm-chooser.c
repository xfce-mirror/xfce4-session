/* $Id$ */
/*-
 * Copyright (c) 2004 Benedikt Meurer <benny@xfce.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-chooser.h>


#define BORDER 6


static void xfsm_chooser_row_activated (GtkTreeView       *treeview,
                                        GtkTreePath       *path,
                                        GtkTreeViewColumn *column,
                                        XfsmChooser       *chooser);
static void xfsm_chooser_realized      (GtkWidget         *widget,
                                        XfsmChooser       *chooser);


enum
{
  PREVIEW_COLUMN,
  NAME_COLUMN,
  TITLE_COLUMN,
  ATIME_COLUMN,
  N_COLUMNS,
};



G_DEFINE_TYPE (XfsmChooser, xfsm_chooser, GTK_TYPE_DIALOG)



void
xfsm_chooser_set_sessions (XfsmChooser *chooser,
                           GList       *sessions,
                           const gchar *default_session)
{
  XfsmSessionInfo *session;
  GtkTreeModel    *model;
  GtkTreeIter      iter;
  gchar           *accessed;
  gchar           *title;
  GList           *lp;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (chooser->tree));
  gtk_list_store_clear (GTK_LIST_STORE (model));

  for (lp = sessions; lp != NULL; lp = lp->next)
    {
      session = (XfsmSessionInfo *) lp->data;

      accessed = g_strdup_printf (_("Last accessed: %s"),
                                  ctime (&session->atime));
      title = g_strdup_printf ("<b><big>%s</big></b>\n"
                               "<small><i>%s</i></small>",
                               session->name, accessed);

      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          PREVIEW_COLUMN, session->preview,
                          NAME_COLUMN, session->name,
                          TITLE_COLUMN, title,
                          ATIME_COLUMN, session->atime,
                          -1);

      g_free (accessed);
      g_free (title);
    }
}


static void
xfsm_chooser_new_session (GtkButton *button,
                          XfsmChooser *chooser)
{
  gtk_dialog_response (GTK_DIALOG (chooser), XFSM_RESPONSE_NEW);
}


static void
xfsm_chooser_logout (GtkButton *button,
                     XfsmChooser *chooser)
{
  gtk_dialog_response (GTK_DIALOG (chooser), GTK_RESPONSE_CANCEL);
}


static void
xfsm_chooser_start_session (GtkButton *button,
                            XfsmChooser *chooser)
{
  gtk_dialog_response (GTK_DIALOG (chooser), XFSM_RESPONSE_LOAD);
}


static void
xfsm_chooser_delete_session (GtkButton *button,
                             XfsmChooser *chooser)
{
  XfceRc *rc;
  gchar *session_file;
  gchar *display_name;
  gchar *resource_name;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeSelection *selection;
  gchar *session;

  display_name = xfsm_gdk_display_get_fullname (gdk_display_get_default ());
  resource_name = g_strconcat ("sessions/xfce4-session-", display_name, NULL);
  session_file = xfce_resource_save_location (XFCE_RESOURCE_CACHE, resource_name, TRUE);

  if (!g_file_test (session_file, G_FILE_TEST_IS_REGULAR))
    {
      g_warning ("xfsm_manager_load_session: Something wrong with %s, Does it exist? Permissions issue?", session_file);
      return;
    }

  /* Remove the session from the treeview */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (chooser->tree));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->tree));
  gtk_tree_selection_get_selected (selection, &model, &iter);
  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  /* Remove the session from session file */
  session = g_strdup_printf ("Session: %s", xfsm_chooser_get_session (chooser));
  rc = xfce_rc_simple_open (session_file, FALSE);
  xfce_rc_delete_group (rc, session, FALSE);
  xfce_rc_close (rc);
  g_free (session);
}


gchar*
xfsm_chooser_get_session (const XfsmChooser *chooser)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GValue            value;
  gchar            *name;

  bzero (&value, sizeof (value));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->tree));
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      g_warning ("xfsm_chooser_get_session: !gtk_tree_selection_get_selected");
      return NULL;
    }
  gtk_tree_model_get_value (model, &iter, NAME_COLUMN, &value);
  name = g_value_dup_string (&value);
  g_value_unset (&value);

  return name;
}


static void
xfsm_chooser_class_init (XfsmChooserClass *klass)
{
}


static void
xfsm_chooser_init (XfsmChooser *chooser)
{
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkListStore *model;
  GtkWidget *button;
  GtkWidget *swin;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *dbox;
  gchar title[256];

  dbox = gtk_dialog_get_content_area(GTK_DIALOG (chooser));

  g_signal_connect_after (G_OBJECT (chooser), "realize",
                          G_CALLBACK (xfsm_chooser_realized), chooser);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 9);
  gtk_box_pack_start (GTK_BOX (dbox), vbox, TRUE, TRUE, 0);
  g_snprintf (title, 256, "<big><b>%s</b></big>",
              _("Session Manager"));
  label = gtk_label_new (title);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_widget_set_margin_bottom (label, 12);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);

  /* scrolled window */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
                                       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (swin),
                                              200);
  gtk_box_pack_start (GTK_BOX (vbox), swin, TRUE, TRUE, 0);

  /* tree view */
  model = gtk_list_store_new (N_COLUMNS,
                              GDK_TYPE_PIXBUF,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_INT);
  chooser->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL(model));
  g_object_unref (G_OBJECT (model));
  gtk_widget_set_tooltip_text (chooser->tree,
                               _("Choose the session you want to restore. "
                                 "You can simply double-click the session "
                                 "name to restore it."));

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (chooser->tree), FALSE);
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "pixbuf", PREVIEW_COLUMN,
                                       NULL);
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "markup", TITLE_COLUMN,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (chooser->tree), column);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->tree));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  g_signal_connect (G_OBJECT (chooser->tree), "row-activated",
                    G_CALLBACK (xfsm_chooser_row_activated), chooser);
  gtk_container_add (GTK_CONTAINER (swin), chooser->tree);
  gtk_widget_set_size_request (chooser->tree, -1, 150);

  /* Session Treeview inline toolbar */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);
  gtk_widget_set_margin_bottom (hbox, 12);
  gtk_style_context_add_class (gtk_widget_get_style_context (hbox), "inline-toolbar");

  /* "New" button */
  button = gtk_button_new_from_icon_name ("list-add-symbolic", GTK_ICON_SIZE_BUTTON);//xfce_gtk_button_new_mixed ("document-new", _("Create New Session"));
  gtk_widget_set_tooltip_text (button, _("Create a new session."));
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (xfsm_chooser_new_session), chooser);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  /* "Delete" button */
  button = gtk_button_new_from_icon_name ("list-remove-symbolic", GTK_ICON_SIZE_BUTTON); //xfce_gtk_button_new_mixed ("document-new", _("Delete Session"));
  gtk_widget_set_tooltip_text (button, _("Delete a saved session."));
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (xfsm_chooser_delete_session), chooser);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  /* Button box */
  hbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_EDGE);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);

  /* "Logout" button */
  button = xfce_gtk_button_new_mixed ("application-exit", _("Log Out"));
  gtk_widget_set_tooltip_text (button,
                               _("Cancel the login attempt and return to "
                                 "the login screen."));
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (xfsm_chooser_logout), chooser);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  /* "Start" button */
  button = xfce_gtk_button_new_mixed ("", _("Start"));
  gtk_widget_set_tooltip_text (button, _("Start an existing session."));
  gtk_style_context_add_class (gtk_widget_get_style_context (button), "suggested-action");
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), XFSM_RESPONSE_LOAD);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (xfsm_chooser_start_session), chooser);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  gtk_widget_show_all (vbox);
}


static void
xfsm_chooser_row_activated (GtkTreeView       *treeview,
                            GtkTreePath       *path,
                            GtkTreeViewColumn *column,
                            XfsmChooser       *chooser)
{
  gtk_dialog_response (GTK_DIALOG (chooser), XFSM_RESPONSE_LOAD);
}


static void
xfsm_chooser_realized (GtkWidget   *widget,
                       XfsmChooser *chooser)
{
  GdkCursor *cursor;

  cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget), GDK_LEFT_PTR);
  gdk_window_set_cursor (gtk_widget_get_window(widget), cursor);
  g_object_unref (cursor);
}
