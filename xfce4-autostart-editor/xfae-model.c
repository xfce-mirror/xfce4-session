/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <xfce4-autostart-editor/xfae-model.h>



typedef struct _XfaeItem XfaeItem;



static void               xfae_model_class_init       (XfaeModelClass     *klass);
static void               xfae_model_init             (XfaeModel          *model);
static void               xfae_model_tree_model_init  (GtkTreeModelIface  *iface);
static void               xfae_model_finalize         (GObject            *object);
static GtkTreeModelFlags  xfae_model_get_flags        (GtkTreeModel       *tree_model);
static gint               xfae_model_get_n_columns    (GtkTreeModel       *tree_model);
static GType              xfae_model_get_column_type  (GtkTreeModel       *tree_model,
                                                       gint                index);
static gboolean           xfae_model_get_iter         (GtkTreeModel       *tree_model,
                                                       GtkTreeIter        *iter,
                                                       GtkTreePath        *path);
static GtkTreePath       *xfae_model_get_path         (GtkTreeModel       *tree_model,
                                                       GtkTreeIter        *iter);
static void               xfae_model_get_value        (GtkTreeModel       *tree_model,
                                                       GtkTreeIter        *iter,
                                                       gint                column,
                                                       GValue             *value);
static gboolean           xfae_model_iter_next        (GtkTreeModel       *tree_model,
                                                       GtkTreeIter        *iter);
static gboolean           xfae_model_iter_children    (GtkTreeModel       *tree_model,
                                                       GtkTreeIter        *iter,
                                                       GtkTreeIter        *parent);
static gboolean           xfae_model_iter_has_child   (GtkTreeModel       *tree_model,
                                                       GtkTreeIter        *iter);
static gint               xfae_model_iter_n_children  (GtkTreeModel       *tree_model,
                                                       GtkTreeIter        *iter);
static gboolean           xfae_model_iter_nth_child   (GtkTreeModel       *tree_model,
                                                       GtkTreeIter        *iter,
                                                       GtkTreeIter        *parent,
                                                       gint                n);
static gboolean           xfae_model_iter_parent      (GtkTreeModel       *tree_model,
                                                       GtkTreeIter        *iter,
                                                       GtkTreeIter        *child);

static XfaeItem          *xfae_item_new               (const gchar        *relpath);
static void               xfae_item_free              (XfaeItem           *item);
static gboolean           xfae_item_is_removable      (XfaeItem           *item);



struct _XfaeModelClass
{
  GObjectClass __parent__;
};

struct _XfaeModel
{
  GObject __parent__;

  guint  stamp;
  GList *items;
};

struct _XfaeItem
{
  gchar     *name;
  GdkPixbuf *icon;
  gchar     *comment;
  gchar     *relpath;
  gboolean   hidden;
};



G_DEFINE_TYPE_WITH_CODE (XfaeModel,
                         xfae_model,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                                xfae_model_tree_model_init));



static void
xfae_model_class_init (XfaeModelClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfae_model_finalize;
}



static void
xfae_model_init (XfaeModel *model)
{
  XfaeItem *item;
  gchar   **files;
  guint     n;

  model->stamp = g_random_int ();

  files = xfce_resource_match (XFCE_RESOURCE_CONFIG, "autostart/*.desktop", TRUE);
  for (n = 0; files[n] != NULL; ++n)
    {
      item = xfae_item_new (files[n]);
      if (G_LIKELY (item != NULL))
        model->items = g_list_append (model->items, item);
    }
  g_strfreev (files);
}



static void
xfae_model_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = xfae_model_get_flags;
  iface->get_n_columns = xfae_model_get_n_columns;
  iface->get_column_type = xfae_model_get_column_type;
  iface->get_iter = xfae_model_get_iter;
  iface->get_path = xfae_model_get_path;
  iface->get_value = xfae_model_get_value;
  iface->iter_next = xfae_model_iter_next;
  iface->iter_children = xfae_model_iter_children;
  iface->iter_has_child = xfae_model_iter_has_child;
  iface->iter_n_children = xfae_model_iter_n_children;
  iface->iter_nth_child = xfae_model_iter_nth_child;
  iface->iter_parent = xfae_model_iter_parent;
}



static void
xfae_model_finalize (GObject *object)
{
  XfaeModel *model = XFAE_MODEL (object);

  /* free all items */
  g_list_foreach (model->items, (GFunc) xfae_item_free, NULL);
  g_list_free (model->items);

  G_OBJECT_CLASS (xfae_model_parent_class)->finalize (object);
}



static GtkTreeModelFlags
xfae_model_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}



static gint
xfae_model_get_n_columns (GtkTreeModel *tree_model)
{
  return XFAE_MODEL_N_COLUMNS;
}



static GType
xfae_model_get_column_type (GtkTreeModel *tree_model,
                            gint          index)
{
  switch (index)
    {
    case XFAE_MODEL_COLUMN_NAME:
      return G_TYPE_STRING;

    case XFAE_MODEL_COLUMN_ICON:
      return GDK_TYPE_PIXBUF;

    case XFAE_MODEL_COLUMN_ENABLED:
      return G_TYPE_BOOLEAN;

    case XFAE_MODEL_COLUMN_REMOVABLE:
      return G_TYPE_BOOLEAN;
    }

  g_assert_not_reached ();
  return G_TYPE_INVALID;
}



static gboolean
xfae_model_get_iter (GtkTreeModel *tree_model,
                     GtkTreeIter  *iter,
                     GtkTreePath  *path)
{
  XfaeModel *model = XFAE_MODEL (tree_model);
  gint       index;

  g_return_val_if_fail (XFAE_IS_MODEL (model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  index = gtk_tree_path_get_indices (path)[0];
  if (G_UNLIKELY (index >= g_list_length (model->items)))
    return FALSE;

  iter->stamp = model->stamp;
  iter->user_data = g_list_nth (model->items, index);

  return TRUE;
}



static GtkTreePath*
xfae_model_get_path (GtkTreeModel *tree_model,
                     GtkTreeIter  *iter)
{
  XfaeModel *model = XFAE_MODEL (tree_model);
  gint       index;

  g_return_val_if_fail (XFAE_IS_MODEL (model), NULL);
  g_return_val_if_fail (model->stamp == iter->stamp, NULL);

  index = g_list_position (model->items, iter->user_data);
  if (G_UNLIKELY (index < 0))
    return NULL;

  return gtk_tree_path_new_from_indices (index, -1);
}



static void
xfae_model_get_value (GtkTreeModel *tree_model,
                      GtkTreeIter  *iter,
                      gint          column,
                      GValue       *value)
{
  XfaeModel *model = XFAE_MODEL (tree_model);
  XfaeItem  *item = ((GList *) iter->user_data)->data;
  gchar     *name;

  g_return_if_fail (XFAE_IS_MODEL (model));
  g_return_if_fail (iter->stamp == model->stamp);

  switch (column)
    {
    case XFAE_MODEL_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      if (G_LIKELY (item->comment != NULL && *item->comment != '\0'))
        name = g_strdup_printf ("%s (%s)", item->name, item->comment);
      else
        name = g_strdup (item->name);
      g_value_take_string (value, name);
      break;

    case XFAE_MODEL_COLUMN_ICON:
      g_value_init (value, GDK_TYPE_PIXBUF);
      g_value_set_object (value, item->icon);
      break;

    case XFAE_MODEL_COLUMN_ENABLED:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, !item->hidden);
      break;

    case XFAE_MODEL_COLUMN_REMOVABLE:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, xfae_item_is_removable (item));
      break;

    default:
      g_assert_not_reached ();
    }
}



static gboolean
xfae_model_iter_next (GtkTreeModel *tree_model,
                      GtkTreeIter  *iter)
{
  XfaeModel *model = XFAE_MODEL (tree_model);

  g_return_val_if_fail (XFAE_IS_MODEL (model), FALSE);
  g_return_val_if_fail (model->stamp == iter->stamp, FALSE);

  iter->user_data = ((GList *) iter->user_data)->next;

  return (iter->user_data != NULL);
}



static gboolean
xfae_model_iter_children (GtkTreeModel *tree_model,
                          GtkTreeIter  *iter,
                          GtkTreeIter  *parent)
{
  XfaeModel *model = XFAE_MODEL (tree_model);

  g_return_val_if_fail (XFAE_IS_MODEL (model), FALSE);

  if (G_LIKELY (parent == NULL && model->items != NULL))
    {
      iter->stamp = model->stamp;
      iter->user_data = model->items;
      return TRUE;
    }

  return FALSE;
}



static gboolean
xfae_model_iter_has_child (GtkTreeModel *tree_model,
                           GtkTreeIter  *iter)
{
  return FALSE;
}



static gint
xfae_model_iter_n_children (GtkTreeModel *tree_model,
                            GtkTreeIter  *iter)
{
  XfaeModel *model = XFAE_MODEL (tree_model);

  g_return_val_if_fail (XFAE_IS_MODEL (model), 0);

  return (iter == NULL) ? g_list_length (model->items) : 0;
}



static gboolean
xfae_model_iter_nth_child (GtkTreeModel *tree_model,
                           GtkTreeIter  *iter,
                           GtkTreeIter  *parent,
                           gint          n)
{
  XfaeModel *model = XFAE_MODEL (tree_model);

  g_return_val_if_fail (XFAE_IS_MODEL (model), FALSE);

  if (G_UNLIKELY (parent == NULL && n < g_list_length (model->items)))
    {
      iter->stamp = model->stamp;
      iter->user_data = g_list_nth (model->items, n);
      return TRUE;
    }

  return FALSE;
}



static gboolean
xfae_model_iter_parent (GtkTreeModel *tree_model,
                        GtkTreeIter  *iter,
                        GtkTreeIter  *child)
{
  return FALSE;
}




static XfaeItem*
xfae_item_new (const gchar *relpath)
{
  const gchar *value;
  XfaeItem    *item = NULL;
  XfceRc      *rc;
  gchar      **args;
  gchar       *icon_name;

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, relpath, TRUE);
  if (G_LIKELY (rc != NULL))
    {
      xfce_rc_set_group (rc, "Desktop Entry");

      /* verify that we have an application here */
      value = xfce_rc_read_entry (rc, "Type", NULL);
      if (G_LIKELY (value != NULL && g_ascii_strcasecmp (value, "Application") == 0))
        {
          item = g_new0 (XfaeItem, 1);
          item->relpath = g_strdup (relpath);

          value = xfce_rc_read_entry (rc, "Name", NULL);
          if (G_LIKELY (value != NULL))
            item->name = g_strdup (value);

          value = xfce_rc_read_entry (rc, "Icon", NULL);
          if (G_UNLIKELY (value != NULL))
            item->icon = xfce_themed_icon_load (value, 16);
          else
            {
              /* we try to be smart here in that we try to guess
               * the icon name from the "Exec" key if no "Icon"
               * key is provided.
               */
              value = xfce_rc_read_entry (rc, "Exec", NULL);
              if (G_LIKELY (value != NULL))
                {
                  args = g_strsplit_set (value, " \t", -1);
                  if (G_LIKELY (*args != NULL))
                    {
                      icon_name = g_path_get_basename (*args);
                      item->icon = xfce_themed_icon_load (icon_name, 16);
                      g_free (icon_name);
                    }
                  g_strfreev (args);
                }
            }

          value = xfce_rc_read_entry (rc, "Comment", NULL);
          if (G_LIKELY (value != NULL))
            item->comment = g_strdup (value);

          item->hidden = xfce_rc_read_bool_entry (rc, "Hidden", FALSE);
        }

      xfce_rc_close (rc);
    }

  return item;
}



static void
xfae_item_free (XfaeItem *item)
{
  if (item->icon != NULL)
    g_object_unref (G_OBJECT (item->icon));

  g_free (item->relpath);
  g_free (item->comment);
  g_free (item->name);
  g_free (item);
}



static gboolean
xfae_item_is_removable (XfaeItem *item)
{
  gboolean removable = TRUE;
  gchar  **files;
  gchar   *dir;
  guint    n;

  /* check whether all of the containing directories are writable */
  files = xfce_resource_lookup_all (XFCE_RESOURCE_CONFIG, item->relpath);
  for (n = 0; files[n] != NULL; ++n)
    {
      dir = g_path_get_dirname (files[n]);
      if (access (dir, R_OK | W_OK | X_OK) < 0)
        removable = FALSE;
      g_free (dir);
    }
  g_strfreev (files);

  return removable;
}



static gboolean
xfae_item_remove (XfaeItem *item,
                  GError  **error)
{
  gboolean succeed = TRUE;
  gchar  **files;
  guint    n;

  /* remove all files that belong to this item */
  files = xfce_resource_lookup_all (XFCE_RESOURCE_CONFIG, item->relpath);
  for (n = 0; files[n] != NULL; ++n)
    {
      if (unlink (files[n]) < 0 && errno != ENOENT)
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                       _("Failed to unlink %s: %s"), files[n], g_strerror (errno));
          succeed = FALSE;
          break;
        }
    }
  g_strfreev (files);

  return succeed;
}



/**
 * xfae_model_new:
 *
 * Allocates a new #XfaeModel instance.
 *
 * Return value: the newly allocated #XfaeModel instance.
 **/
GtkTreeModel*
xfae_model_new (void)
{
  return g_object_new (XFAE_TYPE_MODEL, NULL);
}



/**
 * xfae_model_add:
 * @model       : a #XfaeModel.
 * @name        : the user visible name of the new item.
 * @description : the description for the new item.
 * @command     : the command for the new item.
 * @error       : return locations for errors or %NULL.
 *
 * Attempts to add a new item with the given parameters
 * to @model.
 *
 * Return value: %TRUE if successfull, else %FALSE.
 **/
gboolean
xfae_model_add (XfaeModel   *model,
                const gchar *name,
                const gchar *description,
                const gchar *command,
                GError     **error)
{
  GtkTreePath *path;
  GtkTreeIter  iter;
  XfaeItem    *item;
  XfceRc      *rc;
  gchar       *file;
  gchar       *dir;
  gchar        relpath[4096];
  guint        n;

  g_return_val_if_fail (XFAE_IS_MODEL (model), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (description != NULL, FALSE);
  g_return_val_if_fail (command != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  dir = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "autostart/", TRUE);

  /* generate a unique file name */
  for (n = 0;; ++n)
    {
      file = (n == 0)
        ?  g_strdup_printf ("%s.desktop", name)
        : g_strdup_printf ("%s-%u.desktop", name, n);

      g_snprintf (relpath, 4096, "%s%s", dir, file);
      if (!g_file_test (relpath, G_FILE_TEST_IS_REGULAR))
        break;

      g_free (file);
    }

  /* generate the file spec */
  g_snprintf (relpath, 4096, "autostart/%s", file);
  g_free (file);
  g_free (dir);

  /* generate the .desktop file */
  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, relpath, FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (EIO),
                   _("Failed to create file %s"), relpath);
      return FALSE;
    }

  /* write the content */
  xfce_rc_set_group (rc, "Desktop Entry");
  xfce_rc_write_entry (rc, "Encoding", "UTF-8");
  xfce_rc_write_entry (rc, "Version", "0.9.4");
  xfce_rc_write_entry (rc, "Type", "Application");
  xfce_rc_write_entry (rc, "Name", name);
  xfce_rc_write_entry (rc, "Comment", description);
  xfce_rc_write_entry (rc, "Exec", command);
  xfce_rc_write_bool_entry (rc, "StartupNotify", FALSE);
  xfce_rc_write_bool_entry (rc, "Terminal", FALSE);
  xfce_rc_write_bool_entry (rc, "Hidden", FALSE);
  xfce_rc_close (rc);

  /* now load the matching item for the list */
  item = xfae_item_new (relpath);
  if (G_UNLIKELY (item == NULL))
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (EIO),
                   _("Failed to write file %s"), relpath);
      return FALSE;
    }

  /* append it to our list */
  model->items = g_list_append (model->items, item);

  /* generate the iter for the newly appended item */
  iter.stamp = model->stamp;
  iter.user_data = g_list_last (model->items);

  /* tell the view about it */
  path = gtk_tree_path_new_from_indices (g_list_length (model->items) - 1, -1);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
  gtk_tree_path_free (path);

  return TRUE;
}



/**
 * xfae_model_remove:
 * @model : a #XfaeModel.
 * @iter  : the #GtkTreeIter referring to the item that should be removed.
 * @error : return location for errors or %NULL.
 *
 * Tries to remove the item referred to by @iter from @model.
 *
 * Return value: %TRUE if the removal was successful.
 **/
gboolean
xfae_model_remove (XfaeModel   *model,
                   GtkTreeIter *iter,
                   GError     **error)
{
  GtkTreePath *path;
  XfaeItem    *item;
  GList       *lp;
  gint         index;

  g_return_val_if_fail (XFAE_IS_MODEL (model), FALSE);
  g_return_val_if_fail (iter->stamp == model->stamp, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  lp = iter->user_data;
  item = lp->data;

  /* try to unlink the item from disk */
  if (!xfae_item_remove (item, error))
    return FALSE;

  /* unlink the item from the list */
  index = g_list_position (model->items, lp);
  model->items = g_list_delete_link (model->items, lp);
  xfae_item_free (item);

  /* tell the view that we have just removed one item */
  path = gtk_tree_path_new_from_indices (index, -1);
  gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
  gtk_tree_path_free (path);

  return TRUE;
}



/**
 * xfae_model_toggle:
 * @model : a #XfaeModel.
 * @iter  : the #GtkTreeIter referring to the item that should be toggled.
 * @error : return location for errors or %NULL.
 *
 * Attempts to toggle the state of the item referrred to by @iter.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
xfae_model_toggle (XfaeModel   *model,
                   GtkTreeIter *iter,
                   GError     **error)
{
  GtkTreePath *path;
  XfaeItem    *item;
  XfceRc      *rc;
  GList       *lp;

  g_return_val_if_fail (XFAE_IS_MODEL (model), FALSE);
  g_return_val_if_fail (iter->stamp == model->stamp, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  lp = iter->user_data;
  item = lp->data;

  /* try to open the resource config */
  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, item->relpath, FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (EIO),
                   _("Failed to open %s for writing"), item->relpath);
      return FALSE;
    }

  /* perform the toggle operation :-) */
  item->hidden = !item->hidden;

  /* write the result */
  xfce_rc_set_group (rc, "Desktop Entry");
  xfce_rc_write_bool_entry (rc, "Hidden", item->hidden);
  xfce_rc_close (rc);

  /* tell the view that we have most probably a new state */
  path = gtk_tree_path_new_from_indices (g_list_position (model->items, lp), -1);
  gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, iter);
  gtk_tree_path_free (path);

  return TRUE;
}




