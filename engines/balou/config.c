/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gmodule.h>

#include <libxfcegui4/libxfcegui4.h>

#include <xfce4-session/xfsm-splash-engine.h>

#include <engines/balou/balou-theme.h>
#include <engines/balou/gnome-uri.h>


#define BORDER 6


enum
{
  TARGET_RAW_DATA,
  TARGET_XDS,
  TARGET_URI,
  TARGET_STRING,
};


enum
{
  PREVIEW_COLUMN,
  TITLE_COLUMN,
  NAME_COLUMN,
  N_COLUMNS,
};


static GtkTargetEntry dst_targets[] =
{
  { "text/uri-list", 0, TARGET_URI },
  { "STRING", 0, TARGET_STRING },
};
static gsize dst_ntargets = sizeof (dst_targets) / sizeof (*dst_targets);

static GtkTargetEntry src_targets[] =
{
  { "XdndDirectSave0", 0, TARGET_XDS },
};
static gsize src_ntargets = sizeof (src_targets) / sizeof (*src_targets);


static gboolean
config_load_theme_for_iter (GtkListStore *store,
                            GtkTreeIter  *iter,
                            const gchar  *name)
{
  BalouTheme *theme;
  GdkPixbuf  *preview;
  gchar       title[128];

  theme = balou_theme_load (name);
  if (G_UNLIKELY (theme == NULL))
    return FALSE;

  g_snprintf (title, 128, "<b>%s</b>\n<small><i>%s</i></small>",
              balou_theme_get_name (theme),
              balou_theme_get_description (theme));
  preview = balou_theme_generate_preview (theme, 52, 43);

  gtk_list_store_set (store, iter,
                      PREVIEW_COLUMN, preview,
                      TITLE_COLUMN, title,
                      NAME_COLUMN, name,
                      -1);

  balou_theme_destroy (theme);
  g_object_unref (preview);

  return TRUE;
}


static GtkTreeModel*
config_load_themelist (void)
{
  GtkListStore *store;
  GtkTreeIter   iter;
  gchar       **themes;
  gchar        *name;
  gchar        *endp;
  guint         n;

  store = gtk_list_store_new (N_COLUMNS,
                              GDK_TYPE_PIXBUF,
                              G_TYPE_STRING,
                              G_TYPE_STRING);

  themes = xfce_resource_match (XFCE_RESOURCE_THEMES, "*/balou/themerc", TRUE);
  if (G_LIKELY (themes != NULL))
    {
      for (n = 0; themes[n] != NULL; ++n)
        {
          name = themes[n];
          endp = strchr (name, '/');

          if (G_UNLIKELY (endp == NULL))
            continue;
          else
            *endp = '\0';

          gtk_list_store_append (store, &iter);
          if (!config_load_theme_for_iter (store, &iter, name))
            gtk_list_store_remove (store, &iter);
        }

      g_strfreev (themes);
    }

  return GTK_TREE_MODEL (store);
}


static gboolean
config_find_theme (const gchar *theme_name,
                   GtkTreeView *treeview,
                   GtkTreeIter *iter)
{
  GtkTreeModel *model;
  gboolean      match;
  gchar        *name;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

  if (gtk_tree_model_get_iter_first (model, iter))
    {
      do
        {
          gtk_tree_model_get (model, iter, NAME_COLUMN, &name, -1);
          match = (strcmp (name, theme_name) == 0);
          g_free (name);

          if (match)
            return TRUE;
        }
      while (gtk_tree_model_iter_next (model, iter));
    }

  return FALSE;
}


static void
config_select_theme (const gchar *selected_theme,
                     GtkTreeView *treeview)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  selection = gtk_tree_view_get_selection (treeview);
  model = gtk_tree_view_get_model (treeview);

  if (config_find_theme (selected_theme, treeview, &iter))
    gtk_tree_selection_select_iter (selection, &iter);
  else if (gtk_tree_model_get_iter_first (model, &iter))
    gtk_tree_selection_select_iter (selection, &iter);
}


static gboolean
config_reload_theme (const gchar *name,
                     GtkTreeView *treeview)
{
  GtkListStore *store;
  GtkTreeIter   iter;

  store = GTK_LIST_STORE (gtk_tree_view_get_model (treeview));

  if (!config_find_theme (name, treeview, &iter))
    gtk_list_store_append (store, &iter);

  if (!config_load_theme_for_iter (store, &iter, name))
    {
      gtk_list_store_remove (store, &iter);
      return FALSE;
    }

  config_select_theme (name, treeview);

  return TRUE;
}


static gboolean
config_do_install_theme (const gchar *path,
                         GtkTreeView *treeview)
{
  gchar   *standard_output;
  gchar   *standard_error;
  gint     exit_status;
  gboolean result;
  gchar   *target;
  gchar   *argv[4];

  g_return_val_if_fail (path != NULL, FALSE);

  target = xfce_resource_save_location (XFCE_RESOURCE_THEMES, NULL, TRUE);
  if (G_UNLIKELY (target == NULL))
    {
      g_warning ("Unable to determine save location for themes.");
      return FALSE;
    }

  argv[0] = BALOU_INSTALL_THEME;
  argv[1] = (gchar *) path;
  argv[2] = target;
  argv[3] = NULL;

  result = g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL,
                         &standard_output, &standard_error,
                         &exit_status, NULL);

  g_free (target);

  if (G_UNLIKELY (!result))
    {
      g_warning ("Unable to execute %s", BALOU_INSTALL_THEME);
      return FALSE;
    }

  g_strstrip (standard_output);
  g_strstrip (standard_error);

  if (G_UNLIKELY (exit_status != 0))
    {
      g_warning ("%s failed: %s", BALOU_INSTALL_THEME, standard_error);
      g_free (standard_output);
      g_free (standard_error);
      return FALSE;
    }

  result = config_reload_theme (standard_output, treeview);

  g_free (standard_output);
  g_free (standard_error);
  return result;
}


static void
config_dropped (GtkWidget *treeview, GdkDragContext *context,
                gint x, gint y, GtkSelectionData *data,
                guint info, guint time, gpointer user_data)
{
  gboolean succeed = FALSE;
  GList   *fnames;

  fnames = gnome_uri_list_extract_filenames ((const gchar *) data->data);
  if (fnames != NULL)
    {
      if (g_list_length (fnames) == 1)
        {
          succeed = TRUE;

          if (!config_do_install_theme ((const gchar *) fnames->data,
                                        GTK_TREE_VIEW (treeview)))
            succeed = FALSE;
        }

      gnome_uri_list_free_strings (fnames);
    }

  gtk_drag_finish (context, succeed, FALSE, time);
}


static void
config_install_theme (GtkWidget *item,
                      GtkWidget *menu)
{
  GtkTreeView *treeview;
  GtkWidget   *toplevel;
  GtkWidget   *chooser;
  gchar       *file;

  treeview = GTK_TREE_VIEW (g_object_get_data (G_OBJECT (menu), "tree-view"));
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (treeview));
  chooser = xfce_file_chooser_new (_("Choose theme file to install..."),
                                   GTK_WINDOW (toplevel),
                                   XFCE_FILE_CHOOSER_ACTION_OPEN,
                                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                   GTK_STOCK_OPEN, GTK_RESPONSE_OK,
                                   NULL);
  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_OK)
    {
      file = xfce_file_chooser_get_filename (XFCE_FILE_CHOOSER (chooser));

      if (!config_do_install_theme (file, treeview))
        {
          xfce_err (_("Unable to install splash theme from file %s, "
                      "please check that the file is a valid splash "
                      "theme archive."), file);
        }

      g_free (file);
    }

  gtk_widget_destroy (chooser);
}


#ifdef RM_CMD
static void
config_remove_theme (GtkWidget *item,
                     GtkWidget *menu)
{
  GtkTreeModel *model;
  GtkTreeView  *treeview;
  GtkTreeIter  *iter;
  gboolean      result;
  gchar        *directory;
  gchar        *resource;
  gchar        *name;
  gchar*        argv[4];
  gint          status;

  iter = (GtkTreeIter *) g_object_get_data (G_OBJECT (menu), "iter");
  if (G_UNLIKELY (iter == NULL))
    return;

  treeview = GTK_TREE_VIEW (g_object_get_data (G_OBJECT (menu), "tree-view"));

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

  gtk_tree_model_get (model, iter, NAME_COLUMN, &name, -1);
  if (G_UNLIKELY (name == NULL))
    return;

  resource = g_strconcat (name, "/balou/", NULL);
  directory = xfce_resource_lookup (XFCE_RESOURCE_THEMES, resource);
  g_free (resource);

  argv[0] = RM_CMD;
  argv[1] = "-rf";
  argv[2] = directory;
  argv[3] = NULL;

  result = g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL,
                         NULL, NULL, &status, NULL);

  if (!result || status != 0)
    {
      xfce_err (_("Unable to remove splash theme \"%s\" from directory "
                  "%s."), name, directory);
    }
  else
    {
      gtk_list_store_remove (GTK_LIST_STORE (model), iter);
    }

  g_free (directory);
  g_free (name);
}
#endif


static gboolean
config_do_export_theme (const gchar *name,
                        const gchar *file)
{
  gboolean result;
  gchar   *standard_error;
  gchar   *resource;
  gchar   *themerc;
  gchar   *argv[4];
  gint     status;
  
  resource = g_strconcat (name, "/balou/themerc", NULL);
  themerc = xfce_resource_lookup (XFCE_RESOURCE_THEMES, resource);
  g_free (resource);

  argv[0] = BALOU_EXPORT_THEME;
  argv[1] = themerc;
  argv[2] = (gchar *) file;
  argv[3] = NULL;

  result = g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL,
                         NULL, &standard_error,
                         &status, NULL);

  g_free (themerc);

  if (result)
    {
      g_strstrip (standard_error);

      if (status != 0)
        {
          g_warning ("%s failed: %s", BALOU_EXPORT_THEME, standard_error);
          result = FALSE;
        }

      g_free (standard_error);
    }
  else
    {
      g_warning ("Unable to execute %s", BALOU_EXPORT_THEME);
    }

  return result;
}


static void
config_export_theme (GtkWidget *item,
                     GtkWidget *menu)
{
  GtkTreeModel *model;
  GtkTreeView  *treeview;
  GtkTreeIter  *iter;
  GtkWidget    *toplevel;
  GtkWidget    *dialog;
  gchar        *file;
  gchar        *name;

  iter = (GtkTreeIter *) g_object_get_data (G_OBJECT (menu), "iter");
  if (G_UNLIKELY (iter == NULL))
    return;

  treeview = GTK_TREE_VIEW (g_object_get_data (G_OBJECT (menu), "tree-view"));

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  gtk_tree_model_get (model, iter, NAME_COLUMN, &name, -1);
  if (G_UNLIKELY (name == NULL))
    return;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (treeview));
  dialog = xfce_file_chooser_new (_("Choose theme filename..."),
                                  GTK_WINDOW (toplevel),
                                  XFCE_FILE_CHOOSER_ACTION_SAVE,
                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                  GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                  NULL);
  file = g_strconcat (name, ".tar.gz", NULL);
  xfce_file_chooser_set_current_name (XFCE_FILE_CHOOSER (dialog), file);
  g_free (file);
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      file = xfce_file_chooser_get_filename (XFCE_FILE_CHOOSER (dialog));

      config_do_export_theme (name, file);

      g_free (file);
    }

  gtk_widget_destroy (dialog);
  g_free (name);
}


static void
config_popup_menu (GtkWidget      *treeview,
                   GdkEventButton *event,
                   GtkTreeModel   *model,
                   GtkTreeIter    *iter)
{
  GtkWidget *menu;
  GtkWidget *item;
  gboolean   writable;
  gchar     *directory;
  gchar     *resource;
  gchar     *name;
  guint      button;
  guint      time;

  menu = (GtkWidget *) g_object_get_data (G_OBJECT (treeview), "popup-menu");
  if (GTK_IS_WIDGET (menu))
    {
      if (event != NULL)
        {
          button = event->button;
          time = event->time;
        }
      else
        {
          button = 0;
          time = gtk_get_current_event_time ();
        }

#ifdef RM_CMD
      if (iter != NULL)
        {
          gtk_tree_model_get (model, iter, NAME_COLUMN, &name, -1);

          resource = g_strconcat (name, "/", NULL);
          directory = xfce_resource_lookup (XFCE_RESOURCE_THEMES, resource);
          g_free (resource);

          writable = (access (directory, W_OK) == 0);

          g_free (directory);
          g_free (name);
        }
      else
#endif
        {
          writable = FALSE;
        }

      item = GTK_WIDGET (g_object_get_data (G_OBJECT (menu), "remove-theme"));
      gtk_widget_set_sensitive (item, writable);

      g_object_set_data_full (G_OBJECT (menu), "iter",
                              g_memdup (iter, sizeof (*iter)),
                              (GDestroyNotify) g_free);
      g_object_set_data (G_OBJECT (menu), "tree-view", treeview);

      gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, time);
    }
}


static gboolean
config_button_press_handler (GtkWidget *treeview, GdkEventButton *event)
{
  GtkTreeModel *model;
  GtkTreePath  *path;
  GtkTreeIter   iter;

  if (event->button != 3 || event->type != GDK_BUTTON_PRESS)
    return FALSE;

  if (!gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview), event->x,
        event->y, &path, NULL, NULL, NULL))
    return FALSE;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
  if (!gtk_tree_model_get_iter (model, &iter, path))
    return FALSE;

  config_popup_menu (treeview, event, model, &iter);
  return TRUE;
}


static gboolean
config_popup_menu_handler (GtkWidget *treeview)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return FALSE;

  config_popup_menu (treeview, NULL, model, &iter);
  return TRUE;
}


static GtkWidget*
config_create_popupmenu (void)
{
  GtkWidget *menu;
  GtkWidget *item;
  GtkWidget *image;

  menu = gtk_menu_new ();

  item = gtk_image_menu_item_new_with_mnemonic (_("_Install new theme"));
  image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
  g_object_set_data (G_OBJECT (menu), "install-theme", item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (config_install_theme), menu);
  gtk_widget_show (item);

  item = gtk_image_menu_item_new_with_mnemonic (_("_Remove theme"));
  image = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
  g_object_set_data (G_OBJECT (menu), "remove-theme", item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
#ifdef RM_CMD
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (config_remove_theme), menu);
#else
  gtk_widget_set_sensitive (item, FALSE);
#endif
  gtk_widget_show (item);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  item = gtk_image_menu_item_new_with_mnemonic (_("_Export theme"));
  image = gtk_image_new_from_stock (GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
  g_object_set_data (G_OBJECT (menu), "export-theme", item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  g_signal_connect (G_OBJECT (item), "activate",
                    G_CALLBACK (config_export_theme), menu);
  gtk_widget_show (item);

  return menu;
}


static gchar*
extract_local_path (gchar *uri)
{
  gchar *our_host_name;

	if (*uri == '/')
	{
		gchar    *path;

		if (uri[1] != '/')
			return uri;	/* Just a local path - no host part */

		path = strchr(uri + 2, '/');
		if (!path)
			return NULL;	    /* //something */

		if (path - uri == 2)
			return path;	/* ///path */

    our_host_name = xfce_gethostname ();
		if (strlen(our_host_name) == path - uri - 2 &&
			strncmp(uri + 2, our_host_name, path - uri - 2) == 0)
      {
        g_free (our_host_name);
			  return path;	/* //myhost/path */
      }
    g_free (our_host_name);

		return NULL;	    /* From a different host */
	}
	else
	{
		if (strncasecmp(uri, "file:", 5))
			return NULL;	    /* Don't know this format */

		uri += 5;

		if (*uri == '/')
			return extract_local_path (uri);

		return NULL;
	}
}


static void
config_drag_begin (GtkWidget      *treeview,
                   GdkDragContext *context)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gchar            *filename;
  gchar            *name;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, NAME_COLUMN, &name, -1);
  filename = g_strconcat (name, ".tar.gz", NULL);

  gdk_property_change (context->source_window,
                       gdk_atom_intern ("XdndDirectSave0", FALSE),
                       gdk_atom_intern ("text/plain", FALSE), 8,
                       GDK_PROP_MODE_REPLACE, filename, strlen (filename));

  g_free (filename);
  g_free (name);
}


static void
config_drag_data_get (GtkWidget        *treeview,
                      GdkDragContext   *context,
                      GtkSelectionData *selection_data,
                      guint             info,
                      guint32           time)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gchar            *name;
  guchar           *prop_text;
  gint              prop_len;
  gchar            *to_send = "E";

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, NAME_COLUMN, &name, -1);

  switch (info)
    {
    case TARGET_XDS:
      if (gdk_property_get (context->source_window,
                            gdk_atom_intern ("XdndDirectSave0", FALSE),
                            gdk_atom_intern ("text/plain", FALSE),
                            0, 1024, FALSE, NULL, NULL, &prop_len, &prop_text)
          && prop_text != NULL)
        {
          gchar *localpath;

          /* Zero-Terminate the string */
          prop_text = g_realloc (prop_text, prop_len + 1);
          prop_text[prop_len] = '\0';

          localpath = extract_local_path (prop_text);

          if (localpath != NULL)
            {
              if (config_do_export_theme (name, localpath))
                to_send = "S";
            }
          else
            to_send = "F";

          g_dataset_set_data (context, "XDS-sent", to_send);

          g_free (prop_text);
        }
      break;
    }

  gtk_selection_data_set (selection_data, gdk_atom_intern ("XA_STRING", FALSE),
                          8, to_send, 1);

  g_free (name);
}


static void
config_drag_end (GtkWidget *treeview, GdkDragContext *context)
{
  /* nothing to do here */
}


static void
config_store (GtkTreeView  *treeview,
              XfsmSplashRc *rc)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gchar            *name;

  selection = gtk_tree_view_get_selection (treeview);
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, NAME_COLUMN, &name, -1);
      if (name != NULL)
        {
          xfsm_splash_rc_write_entry (rc, "Theme", name);
          g_free (name);
        }
    }
}


static gboolean
config_selection_changed (GtkTreeSelection *selection,
                          GtkWidget        *treeview)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GdkPixbuf    *icon;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, PREVIEW_COLUMN, &icon, -1);
      gtk_drag_source_set_icon_pixbuf (treeview, icon);
      g_object_unref (icon);
    }

  return FALSE;
}


GtkWidget*
config_create (XfsmSplashRc *rc)
{
  GtkTreeSelection  *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkTreeModel      *model;
  GtkWidget         *treeview;
  GtkWidget         *frame;
  GtkWidget         *popup;
  GtkWidget         *page;
  GtkWidget         *swin;
  GtkWidget         *vbox;
  const gchar       *theme;

  theme = xfsm_splash_rc_read_entry (rc, "Theme", "Default");

  page = gtk_vbox_new (FALSE, BORDER);
  gtk_container_set_border_width (GTK_CONTAINER (page), BORDER);

  frame = xfce_framebox_new (_("Balou theme"), TRUE);
  gtk_box_pack_start (GTK_BOX (page), frame, TRUE, TRUE, 0);
  vbox = gtk_vbox_new (FALSE, 0);
  xfce_framebox_add (XFCE_FRAMEBOX (frame), vbox);

  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), 
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
                                       GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (vbox), swin, TRUE, TRUE, 0);
  gtk_widget_show (swin);

  model = config_load_themelist ();
  treeview = gtk_tree_view_new_with_model (model);
  gtk_widget_set_size_request (treeview, -1, 100);
  g_object_unref (G_OBJECT (model));
  config_select_theme (theme, GTK_TREE_VIEW (treeview));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
                               GTK_SELECTION_SINGLE);
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (config_selection_changed), treeview);
  g_idle_add ((GSourceFunc)config_selection_changed, selection);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
  gtk_container_add (GTK_CONTAINER (swin), treeview);

  /* add tree view columns */
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
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  /* Drag&Drop support (destination) */
  gtk_drag_dest_set (treeview, GTK_DEST_DEFAULT_ALL, dst_targets,
                     dst_ntargets, GDK_ACTION_COPY);
  g_signal_connect (G_OBJECT (treeview), "drag_data_received",
                    G_CALLBACK (config_dropped), NULL);

  /* Drag&Drop support (source) */
  gtk_drag_source_set (treeview, GDK_BUTTON1_MASK,
                       src_targets, src_ntargets,
                       GDK_ACTION_COPY | GDK_ACTION_PRIVATE);
  g_signal_connect (G_OBJECT (treeview), "drag_begin",
                    G_CALLBACK (config_drag_begin), NULL);
  g_signal_connect (G_OBJECT (treeview), "drag_data_get",
                    G_CALLBACK (config_drag_data_get), NULL);
  g_signal_connect (G_OBJECT (treeview), "drag_end",
                    G_CALLBACK (config_drag_end), NULL);

  /* popup menu */
  popup = config_create_popupmenu ();
  g_object_set_data (G_OBJECT (treeview),
                     "popup-menu", popup);
  g_signal_connect (G_OBJECT (treeview), "popup-menu",
                    G_CALLBACK (config_popup_menu_handler), NULL);
  g_signal_connect (G_OBJECT (treeview), "button-press-event",
                    G_CALLBACK (config_button_press_handler), NULL);
  g_signal_connect_swapped (G_OBJECT (treeview), "destroy",
                            G_CALLBACK (gtk_widget_destroy), popup);
  g_signal_connect (G_OBJECT (treeview), "destroy",
                    G_CALLBACK (config_store), rc);

  return page;
}


static void
config_configure (XfsmSplashConfig *config,
                  GtkWidget        *parent)
{
  GtkWidget *dialog;
  GtkWidget *ui;

  dialog = gtk_dialog_new_with_buttons (_("Configure Balou..."),
                                        GTK_WINDOW (parent),
                                        GTK_DIALOG_MODAL
                                        | GTK_DIALOG_NO_SEPARATOR
                                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CLOSE,
                                        GTK_RESPONSE_CLOSE,
                                        NULL);

  ui = config_create (config->rc);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), ui, TRUE, TRUE, 6);
  gtk_widget_show_all (ui);

  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
}


static GdkPixbuf*
config_preview (XfsmSplashConfig *config)
{
  const gchar *name;
  BalouTheme  *theme;
  GdkPixbuf   *pixbuf = NULL;

  name = xfsm_splash_rc_read_entry (config->rc, "Theme", NULL);
  if (G_UNLIKELY (name == NULL))
    return NULL;

  theme = balou_theme_load (name);
  pixbuf = balou_theme_generate_preview (theme, 320, 240);
  balou_theme_destroy (theme);

  return pixbuf;
}


G_MODULE_EXPORT void
config_init (XfsmSplashConfig *config)
{
  config->name        = g_strdup ("Balou");
  config->description = g_strdup ("Balou Splash Engine");
  config->version     = g_strdup (VERSION);
  config->author      = g_strdup ("Benedikt Meurer");
  config->homepage    = g_strdup ("http://www.xfce.org/");

  config->configure   = config_configure;
  config->preview     = config_preview;
}

