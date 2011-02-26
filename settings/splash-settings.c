/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifndef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef XFCE_DISABLE_DEPRECATED
#undef XFCE_DISABLE_DEPRECATED
#endif

#include <xfconf/xfconf.h>

#include <gdk-pixbuf/gdk-pixdata.h>
#include <gmodule.h>
#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>

#include <libxfsm/xfsm-util.h>
#include <libxfsm/xfsm-splash-engine.h>

#include "module.h"
#include "xfce4-session-settings-common.h"

#define SPLASH_ENGINE_PROP  "/splash/Engine"

/*
   Prototypes
 */
static void splash_selection_changed (GtkTreeSelection *selection);


/*
   Declarations
 */
enum
{
  COLUMN_NAME,
  COLUMN_MODULE,
  N_COLUMNS,
};


/*
   Global variables
 */
static GList       *modules = NULL;
static XfceRc      *modules_rc = NULL;
static gboolean     kiosk_can_splash = FALSE;
static gboolean     splash_centered;
static GtkWidget   *splash_dialog = NULL;
static GtkWidget   *splash_treeview;
static GtkWidget   *splash_button_cfg;
static GtkWidget   *splash_button_test;
static GtkWidget   *splash_image;
static GtkWidget   *splash_descr0;
static GtkWidget   *splash_descr1;
static GtkWidget   *splash_version0;
static GtkWidget   *splash_version1;
static GtkWidget   *splash_author0;
static GtkWidget   *splash_author1;
static GtkWidget   *splash_www0;
static GtkWidget   *splash_www1;


/*
   Module loading/unloading
 */
static void
splash_load_modules (void)
{
  const gchar *entry;
  Module      *module;
  gchar       *file;
  GDir        *dir;

  modules_rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG,
                                    "xfce4-session/xfce4-splash.rc",
                                    FALSE);

  dir = g_dir_open (MODULESDIR, 0, NULL);
  if (G_LIKELY (dir != NULL))
    {
      while ((entry = g_dir_read_name (dir)) != NULL)
        {
          if (*entry == '\0' || *entry == '.')
            continue;

          if (!g_str_has_suffix (entry, "." G_MODULE_SUFFIX))
            continue;

          file = g_strconcat (MODULESDIR, G_DIR_SEPARATOR_S, entry, NULL);
          module = module_load (file, SETTINGS_CHANNEL);
          if (G_LIKELY (module != NULL))
            modules = g_list_append (modules, module);
          g_free (file);
        }

      g_dir_close (dir);
    }
}


static void
splash_unload_modules (void)
{
  GList *lp;

  if (G_LIKELY (modules != NULL))
    {
      for (lp = modules; lp != NULL; lp = lp->next)
        module_free (MODULE (lp->data));
      g_list_free (modules);
      modules = NULL;
    }

  if (G_LIKELY (modules_rc != NULL))
    {
      xfce_rc_close (modules_rc);
      modules_rc = NULL;
    }
}


static void
splash_configure (void)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  Module           *module;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (splash_treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, COLUMN_MODULE, &module, -1);
      module_configure (module, splash_dialog);
      splash_selection_changed (selection);
      xfce_rc_flush (modules_rc);
    }
}


static void
splash_test (void)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  Module           *module;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (splash_treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, COLUMN_MODULE, &module, -1);
      gtk_widget_set_sensitive (splash_dialog, FALSE);
      module_test (module, gtk_widget_get_display (splash_dialog));
      gtk_widget_set_sensitive (splash_dialog, TRUE);
    }
}


static void
splash_selection_changed (GtkTreeSelection *selection)
{
  XfconfChannel *channel;
  GtkTreeModel  *model;
  GtkTreeIter    iter;
  const gchar   *str;
  GdkPixbuf     *preview;
  Module        *module;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, COLUMN_MODULE, &module, -1);

      if (module != NULL)
        {
          str = module_descr (module);
          if (G_LIKELY (str != NULL))
            {
              gtk_label_set_text (GTK_LABEL (splash_descr1), str);
              gtk_widget_show (splash_descr0);
              gtk_widget_show (splash_descr1);
            }
          else
            {
              gtk_widget_hide (splash_descr0);
              gtk_widget_hide (splash_descr1);
            }
          gtk_widget_set_sensitive (splash_descr1, TRUE);

          str = module_version (module);
          if (G_LIKELY (str != NULL))
            {
              gtk_label_set_text (GTK_LABEL (splash_version1), str);
              gtk_widget_show (splash_version0);
              gtk_widget_show (splash_version1);
            }
          else
            {
              gtk_widget_hide (splash_version0);
              gtk_widget_hide (splash_version1);
            }
          gtk_widget_set_sensitive (splash_version1, TRUE);

          str = module_author (module);
          if (G_LIKELY (str != NULL))
            {
              gtk_label_set_text (GTK_LABEL (splash_author1), str);
              gtk_widget_show (splash_author0);
              gtk_widget_show (splash_author1);
            }
          else
            {
              gtk_widget_hide (splash_author0);
              gtk_widget_hide (splash_author1);
            }
          gtk_widget_set_sensitive (splash_author1, TRUE);

          str = module_homepage (module);
          if (G_LIKELY (str != NULL))
            {
              gtk_label_set_text (GTK_LABEL (splash_www1), str);
              gtk_widget_show (splash_www0);
              gtk_widget_show (splash_www1);
            }
          else
            {
              gtk_widget_hide (splash_www0);
              gtk_widget_hide (splash_www1);
            }
          gtk_widget_set_sensitive (splash_www1, TRUE);

          preview = module_preview (module);
          if (G_LIKELY (preview != NULL))
            {
              gtk_image_set_from_pixbuf (GTK_IMAGE (splash_image), preview);
              g_object_unref (G_OBJECT (preview));
            }
          else
            {
              gtk_image_set_from_stock (GTK_IMAGE (splash_image),
                                        GTK_STOCK_MISSING_IMAGE,
                                        GTK_ICON_SIZE_DIALOG);
            }

          channel = xfconf_channel_get (SETTINGS_CHANNEL);
          xfconf_channel_set_string (channel, SPLASH_ENGINE_PROP, module_engine (module));

          gtk_widget_set_sensitive (splash_button_cfg, kiosk_can_splash
                                 && module_can_configure (module));
          gtk_widget_set_sensitive (splash_button_test, TRUE);
        }
      else
        {
          gtk_image_set_from_stock (GTK_IMAGE (splash_image),
                                    GTK_STOCK_MISSING_IMAGE,
                                    GTK_ICON_SIZE_DIALOG);

          gtk_label_set_text (GTK_LABEL (splash_descr1), _("None"));
          gtk_widget_set_sensitive (splash_descr1, FALSE);

          gtk_label_set_text (GTK_LABEL (splash_version1), _("None"));
          gtk_widget_set_sensitive (splash_version1, FALSE);

          gtk_label_set_text (GTK_LABEL (splash_author1), _("None"));
          gtk_widget_set_sensitive (splash_author1, FALSE);

          gtk_label_set_text (GTK_LABEL (splash_www1), _("None"));
          gtk_widget_set_sensitive (splash_www1, FALSE);

          gtk_widget_set_sensitive (splash_button_cfg, FALSE);
          gtk_widget_set_sensitive (splash_button_test, FALSE);

          channel = xfconf_channel_get (SETTINGS_CHANNEL);
          xfconf_channel_set_string (channel, SPLASH_ENGINE_PROP, "");
        }
    }

  /* centering must be delayed! */
  if (!splash_centered)
    {
      xfce_gtk_window_center_on_active_screen(GTK_WINDOW(splash_dialog));
      splash_centered = TRUE;
    }
}


static void
splash_dialog_destroy (GtkWidget *widget,
                       gpointer user_data)
{
  splash_unload_modules ();
}


void
splash_settings_init (GtkBuilder *builder)
{
  XfconfChannel     *channel;
  GtkTreeSelection  *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkListStore      *store;
  gchar             *engine;
  GtkTreePath       *path;
  GtkTreeIter        iter;
  XfceKiosk         *kiosk;
  GList             *lp;

  /* load splash modules */
  splash_load_modules ();

  /* query kiosk settings */
  kiosk = xfce_kiosk_new ("xfce4-session");
  kiosk_can_splash = xfce_kiosk_query (kiosk, "Splash") || xfce_kiosk_query (kiosk, "CustomizeSplash");
  xfce_kiosk_free (kiosk);

  /* load config */
  channel = xfconf_channel_get (SETTINGS_CHANNEL);
  engine = xfconf_channel_get_string (channel, SPLASH_ENGINE_PROP, "");

  store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      COLUMN_NAME, _("None"),
                      COLUMN_MODULE, NULL,
                      -1);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);

  for (lp = modules; lp != NULL; lp = lp->next)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          COLUMN_NAME, module_name (MODULE (lp->data)),
                          COLUMN_MODULE, MODULE (lp->data),
                          -1);

      if (strcmp (module_engine (MODULE (lp->data)), engine) == 0)
        {
          gtk_tree_path_free (path);
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
        }
    }

  g_free (engine);

  splash_centered = FALSE;

  splash_treeview = GTK_WIDGET(gtk_builder_get_object (builder, "treeview_splash"));
  gtk_tree_view_set_model (GTK_TREE_VIEW (splash_treeview), GTK_TREE_MODEL (store));
  g_object_unref (G_OBJECT (store));

  /* FIXME: this won't work right when we embed */
  splash_dialog = gtk_widget_get_toplevel (splash_treeview);
  g_signal_connect (G_OBJECT (splash_dialog), "destroy",
                    G_CALLBACK (splash_dialog_destroy), NULL);

  /* add tree view column */
  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", COLUMN_NAME,
                                       NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (splash_treeview), column);

  splash_button_cfg = GTK_WIDGET(gtk_builder_get_object (builder, "btn_splash_configure"));
  g_signal_connect (G_OBJECT (splash_button_cfg), "clicked",
                    splash_configure, NULL);

  splash_button_test = GTK_WIDGET(gtk_builder_get_object (builder, "btn_splash_test"));
  g_signal_connect (G_OBJECT (splash_button_test), "clicked",
                    splash_test, NULL);

  splash_image = GTK_WIDGET(gtk_builder_get_object (builder, "img_splash_preview"));
  gtk_widget_set_size_request (splash_image, 300, 240);

  splash_descr0 = GTK_WIDGET(gtk_builder_get_object (builder, "lbl_splash_desc0"));
  splash_version0 =GTK_WIDGET(gtk_builder_get_object (builder, "lbl_splash_version0"));
  splash_author0 = GTK_WIDGET(gtk_builder_get_object (builder, "lbl_splash_author0"));
  splash_www0 = GTK_WIDGET(gtk_builder_get_object (builder, "lbl_splash_homepage0"));
  splash_descr1 = GTK_WIDGET(gtk_builder_get_object (builder, "lbl_splash_desc1"));
  splash_version1 = GTK_WIDGET(gtk_builder_get_object (builder, "lbl_splash_version1"));
  splash_author1 = GTK_WIDGET(gtk_builder_get_object (builder, "lbl_splash_author1"));
  splash_www1 = GTK_WIDGET(gtk_builder_get_object (builder, "lbl_splash_homepage1"));

  /* handle selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (splash_treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (splash_selection_changed), NULL);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (splash_treeview), path, NULL, FALSE);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (splash_treeview), path, NULL,
                                TRUE, 0.5, 0.0);
  gtk_tree_path_free (path);
}
