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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfcegui4/libxfcegui4.h>

#include <xfce4-session/xfsm-splash-theme.h>

#include <settings/gnome-uri.h>
#include <settings/looknfeel.h>
#include <settings/settings.h>


enum
{
  TARGET_STRING,
  TARGET_URI,
};


enum
{
  PREVIEW_COLUMN,
  TITLE_COLUMN,
  NAME_COLUMN,
  N_COLUMNS,
};


static GtkTargetEntry targets[] =
{
  { "text/uri-list", 0, TARGET_URI },
  { "STRING", 0, TARGET_STRING },
};

static gsize n_targets = sizeof (targets) / sizeof (*targets);
static GtkWidget *looknfeel_treeview;


static gboolean
looknfeel_load_theme_for_iter (GtkListStore *store,
                               GtkTreeIter  *iter,
                               const gchar  *name)
{
  XfsmSplashTheme *theme;
  GdkPixbuf       *preview;
  gchar            title[128];

  theme = xfsm_splash_theme_load (name);
  if (theme == NULL)
    return FALSE;

  g_snprintf (title, 128, "<b>%s</b>\n<small><i>%s</i></small>",
              xfsm_splash_theme_get_name (theme),
              xfsm_splash_theme_get_description (theme));
  preview = xfsm_splash_theme_generate_preview (theme, 52, 43);

  gtk_list_store_set (store, iter,
                      PREVIEW_COLUMN, preview,
                      TITLE_COLUMN, title,
                      NAME_COLUMN, name,
                      -1);

  xfsm_splash_theme_destroy (theme);
  g_object_unref (preview);

  return TRUE;
}


static GtkTreeModel*
looknfeel_load_themelist (void)
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

  themes = xfce_resource_match (XFCE_RESOURCE_THEMES, "*/xfsm4/themerc", TRUE);
  if (themes != NULL)
    {
      for (n = 0; themes[n] != NULL; ++n)
        {
          name = themes[n];
          endp = strchr (name, '/');

          if (endp == NULL)
            continue;
          else
            *endp = '\0';

          gtk_list_store_append (store, &iter);
          if (!looknfeel_load_theme_for_iter (store, &iter, name))
            gtk_list_store_remove (store, &iter);
        }

      g_strfreev (themes);
    }

  return GTK_TREE_MODEL (store);
}


static gboolean
looknfeel_find_theme (const gchar *theme_name, GtkTreeIter *iter)
{
  GtkTreeModel *model;
  gboolean      match;
  gchar        *name;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (looknfeel_treeview));

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
looknfeel_select_theme (const gchar *selected_theme)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (looknfeel_treeview));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (looknfeel_treeview));

  if (looknfeel_find_theme (selected_theme, &iter))
    gtk_tree_selection_select_iter (selection, &iter);
  else if (gtk_tree_model_get_iter_first (model, &iter))
    gtk_tree_selection_select_iter (selection, &iter);
}


static gboolean
looknfeel_reload_theme (const gchar *name)
{
  GtkListStore *store;
  GtkTreeIter   iter;

  store = GTK_LIST_STORE (gtk_tree_view_get_model (
        GTK_TREE_VIEW (looknfeel_treeview)));

  if (!looknfeel_find_theme (name, &iter))
    gtk_list_store_append (store, &iter);

  if (!looknfeel_load_theme_for_iter (store, &iter, name))
    {
      gtk_list_store_remove (store, &iter);
      return FALSE;
    }

  looknfeel_select_theme (name);

  return TRUE;
}


static gboolean
looknfeel_install_theme (const gchar *path)
{
  gchar   *standard_output;
  gchar   *standard_error;
  gint     exit_status;
  gboolean result;
  gchar   *target;
  gchar   *argv[4];

  g_return_val_if_fail (path != NULL, FALSE);

  target = xfce_resource_save_location (XFCE_RESOURCE_THEMES, NULL, TRUE);
  if (target == NULL)
    {
      g_warning ("Unable to determine save location for themes.");
      return FALSE;
    }

  argv[0] = XFSM_INSTALL_THEME;
  argv[1] = (gchar *) path;
  argv[2] = target;
  argv[3] = NULL;

  result = g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL,
                         &standard_output, &standard_error,
                         &exit_status, NULL);

  g_free (target);

  if (!result)
    {
      g_warning ("Unable to execute %s", XFSM_INSTALL_THEME);
      return FALSE;
    }

  g_strstrip (standard_output);
  g_strstrip (standard_error);

  if (exit_status != 0)
    {
      g_warning ("%s failed: %s", XFSM_INSTALL_THEME, standard_error);
      g_free (standard_output);
      g_free (standard_error);
      return FALSE;
    }

  result = looknfeel_reload_theme (standard_output);

  g_free (standard_output);
  g_free (standard_error);
  return result;
}


static void
looknfeel_dropped (GtkWidget *treeview, GdkDragContext *context,
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

          if (!looknfeel_install_theme ((const gchar *) fnames->data))
            succeed = FALSE;
        }

      gnome_uri_list_free_strings (fnames);
    }

  gtk_drag_finish (context, succeed, FALSE, time);
}


GtkWidget*
looknfeel_create (XfceRc *rc)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkWidget        *frame;
  GtkWidget        *page;
  GtkWidget        *swin;
  GtkWidget        *vbox;
  const gchar      *theme;

  xfce_rc_set_group (rc, "Splash Theme");
  theme = xfce_rc_read_entry (rc, "Name", "Default");

  page = gtk_vbox_new (FALSE, BORDER);
  gtk_container_set_border_width (GTK_CONTAINER (page), BORDER);

  frame = xfce_framebox_new (_("Splash theme"), TRUE);
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

  model = looknfeel_load_themelist ();
  looknfeel_treeview = gtk_tree_view_new_with_model (model);
  g_object_unref (G_OBJECT (model));
  looknfeel_select_theme (theme);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (looknfeel_treeview));
  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
                               GTK_SELECTION_SINGLE);
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (config_store), NULL);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (looknfeel_treeview), FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (looknfeel_treeview),
      gtk_tree_view_column_new_with_attributes ("Preview",
        gtk_cell_renderer_pixbuf_new (), "pixbuf", PREVIEW_COLUMN, NULL));
  gtk_tree_view_append_column (GTK_TREE_VIEW (looknfeel_treeview),
      gtk_tree_view_column_new_with_attributes ("Description",
        gtk_cell_renderer_text_new (), "markup", TITLE_COLUMN, NULL));
  gtk_container_add (GTK_CONTAINER (swin), looknfeel_treeview);

  /* Drag&Drop support */
  gtk_drag_dest_set (looknfeel_treeview, GTK_DEST_DEFAULT_ALL, targets,
                     n_targets, GDK_ACTION_COPY);
  g_signal_connect (G_OBJECT (looknfeel_treeview), "drag_data_received",
                    G_CALLBACK (looknfeel_dropped), NULL);

  return page;
}


void
looknfeel_store (XfceRc *rc)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  gchar            *name;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (looknfeel_treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, NAME_COLUMN, &name, -1);
      if (name != NULL)
        {
          xfce_rc_set_group (rc, "Splash Theme");
          xfce_rc_write_entry (rc, "Name", name);
          g_free (name);
        }
    }
}



