/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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

#include "xfae-window.h"
#include "xfae-dialog.h"
#include "xfae-model.h"

#include <xfce4-session/xfsm-global.h>
#include <libxfce4ui/libxfce4ui.h>

static void          xfae_window_add                          (XfaeWindow       *window);
static void          xfae_window_remove                       (XfaeWindow       *window);
static void          xfae_window_edit                         (XfaeWindow       *window);
static gboolean      xfae_window_button_press_event           (GtkWidget        *treeview,
                                                               GdkEventButton   *event,
                                                               XfaeWindow       *window);
static void          xfae_window_item_toggled                 (XfaeWindow       *window,
                                                               gchar            *path_string);
static void          xfae_window_selection_changed            (GtkTreeSelection *selection,
                                                               GtkWidget        *remove_button);
static GtkTreeModel* xfae_window_create_run_hooks_combo_model (void);





struct _XfaeWindowClass
{
  GtkBoxClass __parent__;
};

struct _XfaeWindow
{
  GtkBox            __parent__;

  GtkTreeSelection *selection;

  GtkWidget        *treeview;
};



G_DEFINE_TYPE (XfaeWindow, xfae_window, GTK_TYPE_BOX);



static void
xfae_window_class_init (XfaeWindowClass *klass)
{
}



static void
run_hook_changed (GtkCellRenderer *render,
                  const gchar     *path_str,
                  const gchar     *new_text,
                  gpointer         user_data)
{
    GEnumClass   *klass;
    GEnumValue   *enum_struct;
    GtkTreeView  *treeview = user_data;
    GtkTreeModel *model = gtk_tree_view_get_model (treeview);
    GtkTreePath  *path = gtk_tree_path_new_from_string (path_str);
    GtkTreeIter   iter;
    GError       *error = NULL;

    if (gtk_tree_model_get_iter (model, &iter, path))
      {
        klass = g_type_class_ref (XFSM_TYPE_RUN_HOOK);
        enum_struct = g_enum_get_value_by_nick (klass, new_text);
        g_type_class_unref (klass);
        if (!xfae_model_set_run_hook (model, path, &iter, enum_struct->value, &error))
          {
            xfce_dialog_show_error (NULL, error, _("Failed to set run hook"));
            g_error_free (error);
          }
      }
    gtk_tree_path_free (path);
}



static void
xfae_window_init (XfaeWindow *window)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkTreeModel      *model;
  GtkWidget         *vbox;
  GtkWidget         *hbox;
  GtkWidget         *img;
  GtkWidget         *label;
  GtkWidget         *swin;
  GtkWidget         *bbox;
  GtkWidget         *button;

  vbox = GTK_WIDGET(window);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (vbox),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing (GTK_BOX (vbox), 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 6);
  gtk_widget_show (hbox);

  img = gtk_image_new_from_icon_name ("dialog-information", GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), img, FALSE, FALSE, 0);
  gtk_widget_set_halign (GTK_WIDGET (img), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (img), GTK_ALIGN_START);
  gtk_widget_show (img);

  label = g_object_new (GTK_TYPE_LABEL,
                        "justify", GTK_JUSTIFY_LEFT,
                        "label", _("List of applications that will be started "
                                   "automatically on specific events like login, logout, shutdown, etc.\n"
                                   "On login additionally all applications that were saved "
                                   "on your last logout will be started.\n"
                                   "Cursive applications belong to another desktop environment, "
                                   "but you can still enable them if you want."),
                        "xalign", 0.0f,
                        NULL);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  swin = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                       "hadjustment", NULL,
                       "vadjustment", NULL,
                       "shadow-type", GTK_SHADOW_IN,
                       "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
                       "hscrollbar-policy", GTK_POLICY_AUTOMATIC,
                       NULL);
  gtk_box_pack_start (GTK_BOX (vbox), swin, TRUE, TRUE, 0);
  gtk_widget_show (swin);

  window->treeview = g_object_new (GTK_TYPE_TREE_VIEW,
                                   "headers-visible", FALSE,
                                   "hadjustment", NULL,
                                   "vadjustment", NULL,
                                   NULL);
  g_signal_connect (G_OBJECT (window->treeview), "button-press-event",
                    G_CALLBACK (xfae_window_button_press_event), window);
  gtk_container_add (GTK_CONTAINER (swin), window->treeview);
  gtk_widget_show (window->treeview);

  model = xfae_model_new ();
  gtk_tree_view_set_model (GTK_TREE_VIEW (window->treeview), model);
  gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (window->treeview), XFAE_MODEL_COLUMN_TOOLTIP);
  g_object_unref (G_OBJECT (model));

  window->selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview));
  gtk_tree_selection_set_mode (window->selection, GTK_SELECTION_SINGLE);

  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", FALSE,
                         "resizable", FALSE,
                         NULL);
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect_swapped (G_OBJECT (renderer), "toggled",
                            G_CALLBACK (xfae_window_item_toggled), window);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "active", XFAE_MODEL_COLUMN_ENABLED,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (window->treeview), column);

  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", FALSE,
                         "resizable", FALSE,
                         NULL);
  renderer = gtk_cell_renderer_combo_new ();
  model = xfae_window_create_run_hooks_combo_model ();
  g_object_set (renderer,
                "has-entry", FALSE,
                "model", model,
                "text-column", 0,
                "editable", TRUE,
                "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
                NULL);
  g_signal_connect (renderer, "edited", G_CALLBACK (run_hook_changed), window->treeview);

  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", XFAE_MODEL_RUN_HOOK,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (window->treeview), column);
  g_object_unref (model);
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", FALSE,
                         "resizable", FALSE,
                         NULL);
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "pixbuf", XFAE_MODEL_COLUMN_ICON,
                                       NULL);
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "markup", XFAE_MODEL_COLUMN_NAME,
                                       NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (window->treeview), column);

  bbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, TRUE, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (bbox), "inline-toolbar");
  gtk_widget_show (bbox);

  button = gtk_button_new_with_label (_("Add"));
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
                            G_CALLBACK (xfae_window_add), window);
  gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  button = gtk_button_new_with_label (_("Remove"));
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
                            G_CALLBACK (xfae_window_remove), window);
  gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT (window->selection), "changed",
                    G_CALLBACK (xfae_window_selection_changed), button);
  xfae_window_selection_changed (window->selection, button);

  button = gtk_button_new_with_label (_("Edit"));
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
                            G_CALLBACK (xfae_window_edit), window);
  gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT (window->selection), "changed",
                    G_CALLBACK (xfae_window_selection_changed), button);
  xfae_window_selection_changed (window->selection, button);
}



static GtkTreeModel *
xfae_window_create_run_hooks_combo_model (void)
{
    GtkListStore *ls = gtk_list_store_new (1, G_TYPE_STRING);
    GEnumClass   *klass;
    GEnumValue   *enum_struct;
    GtkTreeIter   iter;
    guint i;

    klass = g_type_class_ref (XFSM_TYPE_RUN_HOOK);
    for (i = 0; i < klass->n_values; ++i)
      {
        gtk_list_store_append (ls, &iter);
        enum_struct = g_enum_get_value (klass, i);
        gtk_list_store_set (ls, &iter, 0, enum_struct->value_nick, -1);
      }
    g_type_class_unref (klass);
    return GTK_TREE_MODEL (ls);
}



static gboolean
xfae_window_button_press_event (GtkWidget      *treeview,
                                GdkEventButton *event,
                                XfaeWindow     *window)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreePath      *path;
  GtkTreeIter       iter;
  GMainLoop        *loop;
  GtkWidget        *menu;
  GtkWidget        *item;
  gboolean          removable = FALSE;

  if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
      if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
                                         event->x, event->y,
                                         &path, NULL, NULL, NULL))
        {
          selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
          gtk_tree_selection_select_path (selection, path);
          gtk_tree_path_free (path);

          if (gtk_tree_selection_get_selected (selection, &model, &iter))
            gtk_tree_model_get (model, &iter, XFAE_MODEL_COLUMN_REMOVABLE, &removable, -1);

          menu = gtk_menu_new ();

          item = gtk_menu_item_new_with_label (_("Add"));
          g_signal_connect_swapped (G_OBJECT (item), "activate",
                                    G_CALLBACK (xfae_window_add), window);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);

          item = gtk_menu_item_new_with_label (_("Remove"));
          g_signal_connect_swapped (G_OBJECT (item), "activate",
                                    G_CALLBACK (xfae_window_remove), window);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_set_sensitive (item, removable);
          gtk_widget_show (item);

          gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (treeview));

          loop = g_main_loop_new (NULL, FALSE);
          g_signal_connect_swapped (G_OBJECT (menu), "deactivate",
                                    G_CALLBACK (g_main_loop_quit), loop);
          gtk_grab_add (menu);
          gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL,
                          NULL, event->button, event->time);
          g_main_loop_run (loop);
          g_main_loop_unref (loop);
          gtk_grab_remove (menu);

          g_object_ref_sink (menu);

          return TRUE;
        }
    }

  return FALSE;
}



static void
xfae_window_add (XfaeWindow *window)
{
  GtkTreeModel *model;
  GtkWidget    *parent;
  GtkWidget    *dialog;
  GError       *error = NULL;
  gchar        *name;
  gchar        *descr;
  gchar        *command;
  XfsmRunHook   run_hook;

  dialog = xfae_dialog_new (NULL, NULL, NULL, XFSM_RUN_HOOK_LOGIN);
  parent = gtk_widget_get_toplevel (GTK_WIDGET (window));
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      gtk_widget_hide (dialog);

      xfae_dialog_get (XFAE_DIALOG (dialog), &name, &descr, &command, &run_hook);

      model = gtk_tree_view_get_model (GTK_TREE_VIEW (window->treeview));
      if (!xfae_model_add (XFAE_MODEL (model), name, descr, command, run_hook, &error))
        {
          xfce_dialog_show_error (GTK_WINDOW (parent), error, _("Failed adding \"%s\""), name);
          g_error_free (error);
        }

      g_free (command);
      g_free (descr);
      g_free (name);
    }
  gtk_widget_destroy (dialog);
}



static void
xfae_window_remove (XfaeWindow *window)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GError           *error = NULL;
  GtkWidget        *parent;
  gchar            *name;
  gboolean          remove_item;

  parent = gtk_widget_get_toplevel (GTK_WIDGET (window));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      if (!xfae_model_get (XFAE_MODEL (model), &iter, &name, NULL, NULL, NULL, &error))
        {
          xfce_dialog_show_error (GTK_WINDOW (parent), error, _("Failed to remove item"));
          g_error_free (error);
          return;
        }
        remove_item = TRUE;
/*      remove_item = xfce_dialog_confirm (GTK_WINDOW (parent), GTK_STOCK_REMOVE, NULL,
                                         _("This will permanently remove the application "
                                           "from the list of automatically started applications"),
                                         _("Are you sure you want to remove \"%s\""), name);
*/
      g_free (name);

      if (remove_item && !xfae_model_remove (XFAE_MODEL (model), &iter, &error))
        {
          xfce_dialog_show_error (GTK_WINDOW (parent), error, _("Failed to remove item"));
          g_error_free (error);
        }
    }
}



static void
xfae_window_edit (XfaeWindow *window)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  GError           *error = NULL;
  gchar            *name;
  gchar            *descr;
  gchar            *command;
  XfsmRunHook       run_hook;
  GtkWidget        *parent;
  GtkWidget        *dialog;

  parent = gtk_widget_get_toplevel (GTK_WIDGET (window));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      if (!xfae_model_get (XFAE_MODEL (model), &iter, &name, &descr, &command, &run_hook, &error))
        {
          xfce_dialog_show_error (GTK_WINDOW (parent), error, _("Failed to edit item"));
          g_error_free (error);
          return;
        }

      dialog = xfae_dialog_new (name, descr, command, run_hook);
      gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));

      g_free (command);
      g_free (descr);
      g_free (name);

      if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
        {
	  gtk_widget_hide (dialog);

	  xfae_dialog_get (XFAE_DIALOG (dialog), &name, &descr, &command, &run_hook);

          if (!xfae_model_edit (XFAE_MODEL (model), &iter, name, descr, command, run_hook, &error))
            {
              xfce_dialog_show_error (GTK_WINDOW (parent), error, _("Failed to edit item \"%s\""), name);
              g_error_free (error);
            }

          g_free (command);
          g_free (descr);
          g_free (name);
        }
      gtk_widget_destroy (dialog);
    }
}


static void
xfae_window_item_toggled (XfaeWindow *window,
                          gchar      *path_string)
{
  GtkTreeModel *model;
  GtkTreePath  *path;
  GtkTreeIter   iter;
  GError       *error = NULL;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (window->treeview));
  path = gtk_tree_path_new_from_string (path_string);
  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      if (!xfae_model_toggle (XFAE_MODEL (model), &iter, &error))
        {
          xfce_dialog_show_error (NULL, error, _("Failed to toggle item"));
          g_error_free (error);
        }
    }
  gtk_tree_path_free (path);
}



static void
xfae_window_selection_changed (GtkTreeSelection *selection,
                               GtkWidget        *remove_button)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gboolean      removable = FALSE;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter, XFAE_MODEL_COLUMN_REMOVABLE, &removable, -1);

  gtk_widget_set_sensitive (remove_button, removable);
}



/**
 * xfae_window_new:
 *
 * Allocates a new #XfaeWindow instance.
 *
 * Return value: the newly allocated #XfaeWindow instance.
 **/
GtkWidget*
xfae_window_new (void)
{
  return g_object_new (XFAE_TYPE_WINDOW, NULL);
}
