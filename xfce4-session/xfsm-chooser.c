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
#include "config.h"
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

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfsm/xfsm-util.h>

#include "xfsm-chooser.h"


#define BORDER 6


static void xfsm_chooser_row_activated (GtkTreeView       *treeview,
                                        GtkTreePath       *path,
                                        GtkTreeViewColumn *column,
                                        XfsmChooser       *chooser);
static void xfsm_chooser_realized      (GtkWidget         *widget,
                                        XfsmChooser       *chooser);



G_DEFINE_TYPE (XfsmChooser, xfsm_chooser, GTK_TYPE_DIALOG)



void
xfsm_chooser_set_sessions (XfsmChooser *chooser,
                           GList       *sessions,
                           const gchar *default_session)
{
  GtkTreeModel    *model;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (chooser->tree));

  settings_list_sessions_populate (GTK_TREE_MODEL (model), sessions);
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
  GtkWidget *button;
  GtkWidget *swin;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *dbox;
  gchar title[256];
  GtkCssProvider *provider;

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

  /* Use Adwaita's keycap style to get a meaningful look out of the box */
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (chooser)), "keycap");
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (chooser)), "xfsm-chooser-dialog");
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, ".xfsm-chooser-dialog { font-size: initial; }", -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (chooser)),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

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
  chooser->tree = gtk_tree_view_new ();
  settings_list_sessions_treeview_init (GTK_TREE_VIEW (chooser->tree));
  gtk_widget_set_tooltip_text (chooser->tree,
                               _("Choose the session you want to restore. "
                                 "You can simply double-click the session "
                                 "name to restore it."));
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
  button = gtk_button_new_from_icon_name ("list-add-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text (button, _("Create a new session."));
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (xfsm_chooser_new_session), chooser);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  /* "Delete" button */
  button = gtk_button_new_from_icon_name ("list-remove-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text (button, _("Delete a saved session."));
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (settings_list_sessions_delete_session), GTK_TREE_VIEW (chooser->tree));
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
