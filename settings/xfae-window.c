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
#include "config.h"
#endif

#include <libxfce4ui/libxfce4ui.h>

#include "xfce4-session/xfsm-global.h"

#include "xfae-dialog.h"
#include "xfae-window.h"

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
static void          xfae_window_correct_treeview_column_size (GtkWidget        *treeview,
                                                               gpointer          user_data);
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
run_hook_changed (GtkCellRendererCombo *combo,
                  char                 *path_str,
                  GtkTreeIter          *combo_iter,
                  gpointer              user_data)
{
    GEnumClass   *klass;
    GEnumValue   *enum_struct;
    GtkTreeView  *treeview = user_data;
    GtkTreeModel *model = gtk_tree_view_get_model (treeview);
    GtkTreePath  *path = gtk_tree_path_new_from_string (path_str);
    GtkTreeIter   iter;
    GError       *error = NULL;
    GtkTreeModel *combo_model;
    gchar        *combo_str;

    if (gtk_tree_model_get_iter (model, &iter, path))
      {
        g_object_get (combo, "model", &combo_model, NULL);
        combo_str = gtk_tree_model_get_string_from_iter (combo_model, combo_iter);
        klass = g_type_class_ref (XFSM_TYPE_RUN_HOOK);
        enum_struct = g_enum_get_value (klass, atoi (combo_str));
        g_type_class_unref (klass);
        g_free (combo_str);
        g_object_unref (combo_model);

        if (enum_struct != NULL
            && !xfae_model_set_run_hook (model, path, &iter, enum_struct->value, &error))
          {
            xfce_dialog_show_error (NULL, error, _("Failed to set run hook"));
            g_error_free (error);
          }
      }
    gtk_tree_path_free (path);
}



static void
xfae_window_correct_treeview_column_size (GtkWidget *treeview,
                                          gpointer   user_data)
{
    gtk_tree_view_columns_autosize (GTK_TREE_VIEW (treeview));
}



static void
xfae_window_init (XfaeWindow *window)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkTreeModel      *model;
  GtkWidget         *vbox;
  GtkWidget         *img;
  GtkWidget         *swin;
  GtkWidget         *bbox;
  GtkWidget         *button;

  vbox = GTK_WIDGET (window);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (vbox),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing (GTK_BOX (vbox), 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

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
  // fix buggy sizing behavior of gtk
  g_signal_connect (G_OBJECT (window->treeview), "realize",
                    G_CALLBACK(xfae_window_correct_treeview_column_size), NULL);

  gtk_container_add (GTK_CONTAINER (swin), window->treeview);
  gtk_widget_show (window->treeview);

  model = xfae_model_new ();
  gtk_tree_view_set_model (GTK_TREE_VIEW (window->treeview), model);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (window->treeview), TRUE);
  gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (window->treeview), XFAE_MODEL_COLUMN_TOOLTIP);
  g_object_unref (G_OBJECT (model));

  window->selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (window->treeview));
  gtk_tree_selection_set_mode (window->selection, GTK_SELECTION_SINGLE);

  // Column "toggled"
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
  gtk_tree_view_column_set_sort_column_id(column, XFAE_MODEL_COLUMN_ENABLED);

  // Column "Program"
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", FALSE,
                         "resizable", FALSE,
                         "expand", TRUE,
                         NULL);
  gtk_tree_view_column_set_title (column, _("Program"));
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "gicon", XFAE_MODEL_COLUMN_ICON,
                                       NULL);
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "markup", XFAE_MODEL_COLUMN_NAME,
                                       NULL);

  gtk_tree_view_column_set_sort_column_id(column, XFAE_MODEL_COLUMN_NAME);

  gtk_tree_view_append_column (GTK_TREE_VIEW (window->treeview), column);


  // Column "Trigger"
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "reorderable", FALSE,
                         "resizable", FALSE,
                         NULL);
  gtk_tree_view_column_set_title (column, _("Trigger"));
  renderer = gtk_cell_renderer_combo_new ();
  model = xfae_window_create_run_hooks_combo_model ();
  g_object_set (renderer,
                "has-entry", FALSE,
                "model", model,
                "text-column", 0,
                "editable", TRUE,
                "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
                NULL);
  g_signal_connect (renderer, "changed", G_CALLBACK (run_hook_changed), window->treeview);

  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", XFAE_MODEL_RUN_HOOK,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (window->treeview), column);
  gtk_tree_view_column_set_sort_column_id(column, XFAE_MODEL_RUN_HOOK);

  g_object_unref (model);

  bbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, TRUE, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (bbox), "inline-toolbar");
  gtk_widget_show (bbox);

  button = gtk_button_new_with_label (_("Add"));
  img = gtk_image_new_from_icon_name ("list-add-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), img);
  gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Add application"));
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
                            G_CALLBACK (xfae_window_add), window);
  gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  button = gtk_button_new_with_label (_("Remove"));
  img = gtk_image_new_from_icon_name ("list-remove-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), img);
  gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Remove application"));
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
                            G_CALLBACK (xfae_window_remove), window);
  gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT (window->selection), "changed",
                    G_CALLBACK (xfae_window_selection_changed), button);
  xfae_window_selection_changed (window->selection, button);

  button = gtk_button_new_with_label (_("Edit"));
  img = gtk_image_new_from_icon_name ("document-edit-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), img);
  gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Edit application"));
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
        gtk_list_store_set (ls, &iter, 0, _(enum_struct->value_nick), -1);
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
          gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *) event);
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
