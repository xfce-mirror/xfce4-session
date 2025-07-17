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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4ui/libxfce4ui.h>

#include "xfae-model.h"

typedef struct _XfaeItem XfaeItem;



static void
xfae_model_tree_model_init (GtkTreeModelIface *iface);
static void
xfae_model_tree_sortable_init (GtkTreeSortableIface *iface);
static void
xfae_model_finalize (GObject *object);
static GtkTreeModelFlags
xfae_model_get_flags (GtkTreeModel *tree_model);
static gint
xfae_model_get_n_columns (GtkTreeModel *tree_model);
static GType
xfae_model_get_column_type (GtkTreeModel *tree_model,
                            gint index_);
static gboolean
xfae_model_get_iter (GtkTreeModel *tree_model,
                     GtkTreeIter *iter,
                     GtkTreePath *path);
static GtkTreePath *
xfae_model_get_path (GtkTreeModel *tree_model,
                     GtkTreeIter *iter);
static void
xfae_model_get_value (GtkTreeModel *tree_model,
                      GtkTreeIter *iter,
                      gint column,
                      GValue *value);
static gboolean
xfae_model_iter_next (GtkTreeModel *tree_model,
                      GtkTreeIter *iter);
static gboolean
xfae_model_iter_children (GtkTreeModel *tree_model,
                          GtkTreeIter *iter,
                          GtkTreeIter *parent);
static gboolean
xfae_model_iter_has_child (GtkTreeModel *tree_model,
                           GtkTreeIter *iter);
static gint
xfae_model_iter_n_children (GtkTreeModel *tree_model,
                            GtkTreeIter *iter);
static gboolean
xfae_model_iter_nth_child (GtkTreeModel *tree_model,
                           GtkTreeIter *iter,
                           GtkTreeIter *parent,
                           gint n);
static gboolean
xfae_model_iter_parent (GtkTreeModel *tree_model,
                        GtkTreeIter *iter,
                        GtkTreeIter *child);
static gboolean
xfae_model_get_sort_column_id (GtkTreeSortable *sortable,
                               int *sort_column_id,
                               GtkSortType *sort_order);
static void
xfae_model_set_sort_column_id (GtkTreeSortable *sortable,
                               int sort_column_id,
                               GtkSortType order);
static void
xfae_model_set_sort_func (GtkTreeSortable *sortable,
                          int sort_column_id,
                          GtkTreeIterCompareFunc func,
                          gpointer data,
                          GDestroyNotify destroy);
static void
xfae_model_set_default_sort_func (GtkTreeSortable *sortable,
                                  GtkTreeIterCompareFunc func,
                                  gpointer data,
                                  GDestroyNotify destroy);
static gboolean
xfae_model_has_default_sort_func (GtkTreeSortable *sortable);
static void
xfae_model_sort (XfaeModel *model);


static XfaeItem *
xfae_item_new (const gchar *relpath);
static void
xfae_item_free (XfaeItem *item);
static gboolean
xfae_item_is_removable (XfaeItem *item);
static gboolean
xfae_item_is_enabled (const XfaeItem *item);
static gint
xfae_item_sort_default (gconstpointer a,
                        gconstpointer b);
static gint
xfae_item_sort_name (gconstpointer a,
                     gconstpointer b);
static gint
xfae_item_sort_enabled (gconstpointer a,
                        gconstpointer b);
static gint
xfae_item_sort_hook (gconstpointer a,
                     gconstpointer b);

GType
xfsm_run_hook_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GEnumValue values[] = {
        { XFSM_RUN_HOOK_LOGIN, "XFSM_RUN_HOOK_LOGIN", N_ ("on login") },
        { XFSM_RUN_HOOK_LOGOUT, "XFSM_RUN_HOOK_LOGOUT", N_ ("on logout") },
        { XFSM_RUN_HOOK_SHUTDOWN, "XFSM_RUN_HOOK_SHUTDOWN", N_ ("on shutdown") },
        { XFSM_RUN_HOOK_RESTART, "XFSM_RUN_HOOK_RESTART", N_ ("on restart") },
        { XFSM_RUN_HOOK_SUSPEND, "XFSM_RUN_HOOK_SUSPEND", N_ ("on suspend") },
        { XFSM_RUN_HOOK_HIBERNATE, "XFSM_RUN_HOOK_HIBERNATE", N_ ("on hibernate") },
        { XFSM_RUN_HOOK_HYBRID_SLEEP, "XFSM_RUN_HOOK_HYBRID_SLEEP", N_ ("on hybrid sleep") },
        { XFSM_RUN_HOOK_SWITCH_USER, "XFSM_RUN_HOOK_SWITCH_USER", N_ ("on switch user") },
        { 0, NULL, NULL },
      };
      type = g_enum_register_static ("XfsmRunHook", values);
    }
  return type;
}

struct _XfaeModelClass
{
  GObjectClass __parent__;
};

struct _XfaeModel
{
  GObject __parent__;

  gint stamp;
  GList *items;
  int sort_column_id;
  GtkSortType sort_order;
};

struct _XfaeItem
{
  gchar *name;
  GIcon *icon;
  gchar *comment;
  gchar *relpath;
  gboolean hidden;
  gchar *tooltip;
  XfsmRunHook run_hook;

  gboolean show_in_xfce;
  gboolean show_in_override;
};



G_DEFINE_TYPE_WITH_CODE (XfaeModel,
                         xfae_model,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                                xfae_model_tree_model_init);
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE,
                                                xfae_model_tree_sortable_init));



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
  gchar **files;
  guint n;

  model->sort_column_id = GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID;
  model->sort_order = GTK_SORT_DESCENDING;

  model->stamp = g_random_int ();

  files = xfce_resource_match (XFCE_RESOURCE_CONFIG, "autostart/*.desktop", TRUE);
  for (n = 0; files[n] != NULL; ++n)
    {
      item = xfae_item_new (files[n]);
      if (G_LIKELY (item != NULL))
        model->items = g_list_prepend (model->items, item);
    }
  g_strfreev (files);

  model->items = g_list_sort (model->items, xfae_item_sort_default);
}



static gboolean
xfae_model_get_sort_column_id (GtkTreeSortable *sortable,
                               int *sort_column_id,
                               GtkSortType *sort_order)
{
  XfaeModel *model = XFAE_MODEL (sortable);
  if (sort_column_id)
    *sort_column_id = model->sort_column_id;
  if (sort_order)
    *sort_order = model->sort_order;

  if (model->sort_column_id == GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID
      || model->sort_column_id == GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID)
    {
      return FALSE;
    }

  return TRUE;
}



static void
xfae_model_set_sort_column_id (GtkTreeSortable *sortable,
                               int sort_column_id,
                               GtkSortType order)
{
  XfaeModel *model = XFAE_MODEL (sortable);

  if (model->sort_column_id == sort_column_id && model->sort_order == order)
    return;

  if (GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID == sort_column_id
      || GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID == sort_column_id)
    {
      // This path is probably never hit
      return;
    }

  model->sort_column_id = sort_column_id;
  model->sort_order = order;

  xfae_model_sort (model);

  gtk_tree_sortable_sort_column_changed (sortable);
}



static void
xfae_model_set_sort_func (GtkTreeSortable *sortable,
                          int sort_column_id,
                          GtkTreeIterCompareFunc func,
                          gpointer data,
                          GDestroyNotify destroy)
{
  g_warning ("xfae_model_set_sort_func not supported");
  return;
}



static void
xfae_model_set_default_sort_func (GtkTreeSortable *sortable,
                                  GtkTreeIterCompareFunc func,
                                  gpointer data,
                                  GDestroyNotify destroy)
{
  g_warning ("xfae_model_set_default_sort_func not supported");
  return;
}



static gboolean
xfae_model_has_default_sort_func (GtkTreeSortable *sortable)
{
  return FALSE;
}



static void
xfae_model_sort (XfaeModel *model)
{
  // sort ASC
  switch (model->sort_column_id)
    {
    case XFAE_MODEL_COLUMN_NAME:
      model->items = g_list_sort (model->items, xfae_item_sort_name);
      break;
    case XFAE_MODEL_COLUMN_ENABLED:
      model->items = g_list_sort (model->items, xfae_item_sort_enabled);
      break;
    case XFAE_MODEL_RUN_HOOK:
      model->items = g_list_sort (model->items, xfae_item_sort_hook);
      break;
    default:
      return;
    }

  // If necessary, reverse so we have DESC.
  // This implementation is inefficient, but this is only rarely called.
  if (GTK_SORT_DESCENDING == model->sort_order)
    {
      model->items = g_list_reverse (model->items);
    }
}



static void
xfae_model_tree_sortable_init (GtkTreeSortableIface *iface)
{
  iface->get_sort_column_id = xfae_model_get_sort_column_id;
  iface->set_sort_column_id = xfae_model_set_sort_column_id;
  iface->set_sort_func = xfae_model_set_sort_func;
  iface->set_default_sort_func = xfae_model_set_default_sort_func;
  iface->has_default_sort_func = xfae_model_has_default_sort_func;
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
  g_list_foreach (model->items, (GFunc) G_CALLBACK (xfae_item_free), NULL);
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
                            gint index_)
{
  switch (index_)
    {
    case XFAE_MODEL_COLUMN_NAME:
    case XFAE_MODEL_COLUMN_TOOLTIP:
    case XFAE_MODEL_RUN_HOOK:
      return G_TYPE_STRING;

    case XFAE_MODEL_COLUMN_ICON:
      return G_TYPE_ICON;

    case XFAE_MODEL_COLUMN_ENABLED:
    case XFAE_MODEL_COLUMN_REMOVABLE:
      return G_TYPE_BOOLEAN;
    }

  g_assert_not_reached ();
  return G_TYPE_INVALID;
}



static gboolean
xfae_model_get_iter (GtkTreeModel *tree_model,
                     GtkTreeIter *iter,
                     GtkTreePath *path)
{
  XfaeModel *model = XFAE_MODEL (tree_model);
  guint index_;

  g_return_val_if_fail (XFAE_IS_MODEL (model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  index_ = gtk_tree_path_get_indices (path)[0];
  if (G_UNLIKELY (index_ >= g_list_length (model->items)))
    return FALSE;

  iter->stamp = model->stamp;
  iter->user_data = g_list_nth (model->items, index_);

  return TRUE;
}



static GtkTreePath *
xfae_model_get_path (GtkTreeModel *tree_model,
                     GtkTreeIter *iter)
{
  XfaeModel *model = XFAE_MODEL (tree_model);
  gint index_;

  g_return_val_if_fail (XFAE_IS_MODEL (model), NULL);
  g_return_val_if_fail (model->stamp == iter->stamp, NULL);

  index_ = g_list_position (model->items, iter->user_data);
  if (G_UNLIKELY (index_ < 0))
    return NULL;

  return gtk_tree_path_new_from_indices (index_, -1);
}



gboolean
xfae_model_set_run_hook (GtkTreeModel *tree_model,
                         GtkTreePath *path,
                         GtkTreeIter *iter,
                         XfsmRunHook run_hook,
                         GError **error)
{
  XfaeItem *item = ((GList *) iter->user_data)->data;
  XfceRc *rc;

  /* try to open the resource config */
  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, item->relpath, FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (EIO),
                   _("Failed to open %s for writing"), item->relpath);
      return FALSE;
    }

  xfce_rc_set_group (rc, "Desktop Entry");
  item->run_hook = run_hook;
  xfce_rc_write_int_entry (rc, "RunHook", item->run_hook);
  xfce_rc_close (rc);

  /* tell the view that we have most probably a new state */
  gtk_tree_model_row_changed (tree_model, path, iter);

  return TRUE;
}



static void
xfae_model_get_value (GtkTreeModel *tree_model,
                      GtkTreeIter *iter,
                      gint column,
                      GValue *value)
{
  XfaeModel *model = XFAE_MODEL (tree_model);
  XfaeItem *item = ((GList *) iter->user_data)->data;
  gchar *name;
  gchar *cursive;
  GEnumClass *klass;
  GEnumValue *enum_struct;

  g_return_if_fail (XFAE_IS_MODEL (model));
  g_return_if_fail (iter->stamp == model->stamp);

  switch (column)
    {
    case XFAE_MODEL_COLUMN_NAME:
      g_value_init (value, G_TYPE_STRING);
      if (G_LIKELY (item->comment != NULL && *item->comment != '\0'))
        name = g_markup_printf_escaped ("%s (%s)", item->name, item->comment);
      else
        name = g_markup_printf_escaped ("%s", item->name);

      if (!item->show_in_xfce)
        {
          cursive = g_strdup_printf ("<i>%s</i>", name);
          g_free (name);
          name = cursive;
        }

      g_value_take_string (value, name);
      break;

    case XFAE_MODEL_COLUMN_ICON:
      g_value_init (value, G_TYPE_ICON);
      g_value_set_object (value, item->icon);
      break;

    case XFAE_MODEL_COLUMN_ENABLED:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, xfae_item_is_enabled (item));
      break;

    case XFAE_MODEL_COLUMN_REMOVABLE:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, xfae_item_is_removable (item));
      break;

    case XFAE_MODEL_COLUMN_TOOLTIP:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, item->tooltip);
      break;

    case XFAE_MODEL_RUN_HOOK:
      g_value_init (value, G_TYPE_STRING);
      klass = g_type_class_ref (XFSM_TYPE_RUN_HOOK);
      enum_struct = g_enum_get_value (klass, item->run_hook);
      g_type_class_unref (klass);
      g_value_set_static_string (value, _(enum_struct->value_nick));
      break;

    default:
      g_assert_not_reached ();
    }
}



static gboolean
xfae_model_iter_next (GtkTreeModel *tree_model,
                      GtkTreeIter *iter)
{
  XfaeModel *model = XFAE_MODEL (tree_model);

  g_return_val_if_fail (XFAE_IS_MODEL (model), FALSE);
  g_return_val_if_fail (model->stamp == iter->stamp, FALSE);

  iter->user_data = ((GList *) iter->user_data)->next;

  return (iter->user_data != NULL);
}



static gboolean
xfae_model_iter_children (GtkTreeModel *tree_model,
                          GtkTreeIter *iter,
                          GtkTreeIter *parent)
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
                           GtkTreeIter *iter)
{
  return FALSE;
}



static gint
xfae_model_iter_n_children (GtkTreeModel *tree_model,
                            GtkTreeIter *iter)
{
  XfaeModel *model = XFAE_MODEL (tree_model);

  g_return_val_if_fail (XFAE_IS_MODEL (model), 0);

  return (iter == NULL) ? g_list_length (model->items) : 0;
}



static gboolean
xfae_model_iter_nth_child (GtkTreeModel *tree_model,
                           GtkTreeIter *iter,
                           GtkTreeIter *parent,
                           gint n)
{
  XfaeModel *model = XFAE_MODEL (tree_model);

  g_return_val_if_fail (XFAE_IS_MODEL (model), FALSE);

  if (G_UNLIKELY (parent == NULL && n < (gint) g_list_length (model->items)))
    {
      iter->stamp = model->stamp;
      iter->user_data = g_list_nth (model->items, n);
      return TRUE;
    }

  return FALSE;
}



static gboolean
xfae_model_iter_parent (GtkTreeModel *tree_model,
                        GtkTreeIter *iter,
                        GtkTreeIter *child)
{
  return FALSE;
}



static XfaeItem *
xfae_item_new (const gchar *relpath)
{
  const gchar *value;
  XfaeItem *item = NULL;
  gboolean skip = FALSE;
  XfceRc *rc;
  gchar **only_show_in;
  gchar **not_show_in;
  gchar **args;
  gint m;
  gchar *command;

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, relpath, TRUE);
  if (G_LIKELY (rc != NULL))
    {
      xfce_rc_set_group (rc, "Desktop Entry");

      /* verify that we have an application here */
      value = xfce_rc_read_entry (rc, "Type", NULL);
      if (G_LIKELY (value != NULL
                    && g_ascii_strcasecmp (value, "Application") == 0))
        {
          item = g_new0 (XfaeItem, 1);
          item->relpath = g_strdup (relpath);

          value = xfce_rc_read_entry (rc, "Name", NULL);
          if (G_LIKELY (value != NULL))
            item->name = g_strdup (value);

          value = xfce_rc_read_entry (rc, "Icon", "application-x-executable");
          if (G_LIKELY (value != NULL))
            item->icon = g_themed_icon_new_with_default_fallbacks (value);

          value = xfce_rc_read_entry (rc, "Comment", NULL);
          if (G_LIKELY (value != NULL))
            item->comment = g_strdup (value);

          value = xfce_rc_read_entry (rc, "Exec", NULL);
          if (G_LIKELY (value != NULL))
            item->tooltip = g_markup_printf_escaped ("<b>%s</b> %s", _("Command:"), value);

          item->run_hook = xfce_rc_read_int_entry (rc, "RunHook", XFSM_RUN_HOOK_LOGIN);
          item->hidden = xfce_rc_read_bool_entry (rc, "Hidden", FALSE);
        }
      else
        {
          xfce_rc_close (rc);
          return NULL;
        }

      item->show_in_override = xfce_rc_read_bool_entry (rc, "X-XFCE-Autostart-Override", FALSE);

      /* check the NotShowIn setting */
      not_show_in = xfce_rc_read_list_entry (rc, "NotShowIn", ";");
      if (G_UNLIKELY (not_show_in != NULL))
        {
          /* check if "XFCE" is specified */
          for (m = 0; not_show_in[m] != NULL; ++m)
            if (g_ascii_strcasecmp (not_show_in[m], "XFCE") == 0)
              {
                skip = TRUE;
                break;
              }

          g_strfreev (not_show_in);
        }

      /* check the OnlyShowIn setting */
      only_show_in = xfce_rc_read_list_entry (rc, "OnlyShowIn", ";");
      if (G_UNLIKELY (only_show_in != NULL))
        {
          /* check if "XFCE" is specified */
          for (m = 0; only_show_in[m] != NULL; ++m)
            if (g_ascii_strcasecmp (only_show_in[m], "XFCE") == 0)
              {
                item->show_in_xfce = TRUE;
                break;
              }

          g_strfreev (only_show_in);
        }
      else
        {
          /* no OnlyShowIn, treat it like a normal application */
          item->show_in_xfce = TRUE;
        }

      value = xfce_rc_read_entry (rc, "TryExec", NULL);
      if (value != NULL && g_shell_parse_argv (value, NULL, &args, NULL))
        {
          if (!g_file_test (args[0], G_FILE_TEST_EXISTS))
            {
              command = g_find_program_in_path (args[0]);
              if (command == NULL)
                skip = TRUE;
              g_free (command);
            }

          g_strfreev (args);
        }

      xfce_rc_close (rc);

      /* check if we should skip the item */
      if (G_UNLIKELY (skip))
        {
          xfae_item_free (item);
          item = NULL;
        }
    }

  return item;
}



static void
xfae_item_free (XfaeItem *item)
{
  if (G_UNLIKELY (item == NULL))
    return;

  if (item->icon != NULL)
    g_object_unref (G_OBJECT (item->icon));

  g_free (item->relpath);
  g_free (item->comment);
  g_free (item->name);
  g_free (item->tooltip);
  g_free (item);
}



static gboolean
xfae_item_is_removable (XfaeItem *item)
{
  gboolean removable = TRUE;
  gchar **files;
  gchar *dir;
  guint n;

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
                  GError **error)
{
  gboolean succeed = TRUE;
  gchar **files;
  guint n;

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
 *  Essentially determines, if the "toggle" value in the treeview
 *   is true or false
 **/
static gboolean
xfae_item_is_enabled (const XfaeItem *item)
{
  if (item->show_in_xfce)
    return !item->hidden;

  return !item->hidden && item->show_in_override;
}



/**
 * Initial sorting given, when list is created first
 **/
static gint
xfae_item_sort_default (gconstpointer a,
                        gconstpointer b)
{
  const XfaeItem *item_a = a;
  const XfaeItem *item_b = b;

  /* sort non-xfce items below */
  if (item_a->show_in_xfce != item_b->show_in_xfce)
    return item_a->show_in_xfce ? -1 : 1;

  return g_utf8_collate (item_a->name, item_b->name);
}



static gint
xfae_item_sort_name (gconstpointer a,
                     gconstpointer b)
{
  const XfaeItem *item_a = a;
  const XfaeItem *item_b = b;

  return g_utf8_collate (item_a->name, item_b->name);
}



static gint
xfae_item_sort_enabled (gconstpointer a,
                        gconstpointer b)
{
  const XfaeItem *item_a = a;
  const XfaeItem *item_b = b;

  /* Ordering: ASC = true,false; DESC = false,true */
  return (xfae_item_is_enabled (item_b) - xfae_item_is_enabled (item_a));
}



static gint
xfae_item_sort_hook (gconstpointer a,
                     gconstpointer b)
{
  const XfaeItem *item_a = a;
  const XfaeItem *item_b = b;

  return (item_a->run_hook - item_b->run_hook);
}



/**
 * xfae_model_new:
 *
 * Allocates a new #XfaeModel instance.
 *
 * Return value: the newly allocated #XfaeModel instance.
 **/
GtkTreeModel *
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
 * @run_hook    : hook/trigger on which the command should be executed.
 * @error       : return locations for errors or %NULL.
 *
 * Attempts to add a new item with the given parameters
 * to @model.
 *
 * Return value: %TRUE if successfull, else %FALSE.
 **/
gboolean
xfae_model_add (XfaeModel *model,
                const gchar *name,
                const gchar *description,
                const gchar *command,
                XfsmRunHook run_hook,
                GError **error)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  XfaeItem *item;
  XfceRc *rc;
  gchar *file;
  gchar *dir;
  gchar relpath[4096];
  guint n;

  g_return_val_if_fail (XFAE_IS_MODEL (model), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (description != NULL, FALSE);
  g_return_val_if_fail (command != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  dir = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "autostart/", TRUE);

  /* generate a unique file name */
  for (n = 0;; ++n)
    {
      file = (n == 0) ? g_strdup_printf ("%s.desktop", name)
                      : g_strdup_printf ("%s-%u.desktop", name, n);
      file = g_strdelimit (file, G_DIR_SEPARATOR_S, '-'); /* not a copy */

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
  xfce_rc_write_entry (rc, "OnlyShowIn", "XFCE;");
  xfce_rc_write_int_entry (rc, "RunHook", run_hook);
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
 * xfae_model_get:
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
xfae_model_get (XfaeModel *model,
                GtkTreeIter *iter,
                gchar **name,
                gchar **description,
                gchar **command,
                XfsmRunHook *run_hook,
                GError **error)
{
  XfaeItem *item;
  GList *lp;
  XfceRc *rc;
  const gchar *value;

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
                   _("Failed to open %s for reading"), item->relpath);
      return FALSE;
    }

  xfce_rc_set_group (rc, "Desktop Entry");

  /* read the resource config */
  value = xfce_rc_read_entry (rc, "Name", NULL);
  if (name != NULL)
    *name = g_strdup (value);

  value = xfce_rc_read_entry (rc, "Comment", NULL);
  if (description != NULL)
    *description = g_strdup (value);

  value = xfce_rc_read_entry (rc, "Exec", NULL);
  if (command != NULL)
    *command = g_strdup (value);

  if (run_hook != NULL)
    *run_hook = xfce_rc_read_int_entry (rc, "RunHook", XFSM_RUN_HOOK_LOGIN);

  xfce_rc_close (rc);

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
xfae_model_remove (XfaeModel *model,
                   GtkTreeIter *iter,
                   GError **error)
{
  GtkTreePath *path;
  XfaeItem *item;
  GList *lp;
  gint index_;

  g_return_val_if_fail (XFAE_IS_MODEL (model), FALSE);
  g_return_val_if_fail (iter->stamp == model->stamp, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  lp = iter->user_data;
  item = lp->data;

  /* try to unlink the item from disk */
  if (!xfae_item_remove (item, error))
    return FALSE;

  /* unlink the item from the list */
  index_ = g_list_position (model->items, lp);
  model->items = g_list_delete_link (model->items, lp);
  xfae_item_free (item);

  /* tell the view that we have just removed one item */
  path = gtk_tree_path_new_from_indices (index_, -1);
  gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
  gtk_tree_path_free (path);

  return TRUE;
}



/**
 * xfae_model_edit:
 * @model       : a #XfaeModel.
 * @iter        : the #GtkTreeIter referring to the item that should be removed.
 * @name        : the user visible name of the new item.
 * @description : the description for the new item.
 * @command     : the command for the new item.
 * @run_hook    : hook/trigger on which the command should be executed.
 * @error       : return locations for errors or %NULL.
 *
 * Attempts to edit an item with the given parameters
 * to @model.
 *
 * Return value: %TRUE if successfull, else %FALSE.
 **/
gboolean
xfae_model_edit (XfaeModel *model,
                 GtkTreeIter *iter,
                 const gchar *name,
                 const gchar *description,
                 const gchar *command,
                 XfsmRunHook run_hook,
                 GError **error)
{
  GtkTreePath *path;
  XfaeItem *item;
  XfceRc *rc;
  GList *lp;

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

  item->name = g_strdup (name);
  item->comment = g_strdup (description);

  /* write the result */
  xfce_rc_set_group (rc, "Desktop Entry");
  xfce_rc_write_entry (rc, "Name", name);
  xfce_rc_write_entry (rc, "Comment", description);
  xfce_rc_write_entry (rc, "Exec", command);
  xfce_rc_write_int_entry (rc, "RunHook", run_hook);
  xfce_rc_close (rc);

  /* tell the view that we have most probably a new state */
  path = gtk_tree_path_new_from_indices (g_list_position (model->items, lp), -1);
  gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, iter);
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
xfae_model_toggle (XfaeModel *model,
                   GtkTreeIter *iter,
                   GError **error)
{
  GtkTreePath *path;
  XfaeItem *item;
  XfceRc *rc;
  GList *lp;

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

  xfce_rc_set_group (rc, "Desktop Entry");

  if (item->show_in_xfce)
    {
      /* This is an application with no OnlyShowIn categories or with
       * XFCE in it. In this case, toggle the Hidden flag */

      item->hidden = !item->hidden;
      xfce_rc_write_bool_entry (rc, "Hidden", item->hidden);
    }
  else
    {
      /* Normally a non-Xfce autostart application, toggle the override
       * boolean so we don't hide the service in other desktops */

      item->show_in_override = !item->show_in_override;
      xfce_rc_write_bool_entry (rc, "X-XFCE-Autostart-Override", item->show_in_override);

      /* if we override, but the item is still hidden, toggle that as well then */
      if (item->hidden && item->show_in_override)
        {
          item->hidden = !item->hidden;
          xfce_rc_write_bool_entry (rc, "Hidden", item->hidden);
        }
    }

  xfce_rc_close (rc);

  /* tell the view that we have most probably a new state */
  path = gtk_tree_path_new_from_indices (g_list_position (model->items, lp), -1);
  gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, iter);
  gtk_tree_path_free (path);

  return TRUE;
}
