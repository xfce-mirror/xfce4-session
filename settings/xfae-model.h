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

#ifndef __XFAE_MODEL_H__
#define __XFAE_MODEL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS;

typedef struct _XfaeModelClass XfaeModelClass;
typedef struct _XfaeModel XfaeModel;

#define XFAE_TYPE_MODEL (xfae_model_get_type ())
#define XFAE_MODEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFAE_TYPE_MODEL, XfaeModel))
#define XFAE_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), XFAE_TYPE_MODEL, XfaeModelClass))
#define XFAE_IS_MODEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFAE_TYPE_MODEL))
#define XFAE_IS_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFAE_TYPE_MODEL))
#define XFAE_MODEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), XFAE_TYPE_MODEL, XfaeModelClass))

#define XFSM_TYPE_RUN_HOOK (xfsm_run_hook_get_type ())

/**
 * XfsmRunHook:
 * The trigger / hook on which a specific command should be executed
 **/
typedef enum
{
  XFSM_RUN_HOOK_LOGIN,
  XFSM_RUN_HOOK_LOGOUT,
  XFSM_RUN_HOOK_SHUTDOWN,
  XFSM_RUN_HOOK_RESTART,
  XFSM_RUN_HOOK_SUSPEND,
  XFSM_RUN_HOOK_HIBERNATE,
  XFSM_RUN_HOOK_HYBRID_SLEEP,
  XFSM_RUN_HOOK_SWITCH_USER,
} XfsmRunHook;

GType
xfsm_run_hook_get_type (void) G_GNUC_CONST;

/**
 * XfaeModelColumn:
 *
 * Columns exported by the #XfaeModelColumn using
 * the #GtkTreeModel interface.
 **/
typedef enum
{
  XFAE_MODEL_COLUMN_ICON,
  XFAE_MODEL_COLUMN_NAME,
  XFAE_MODEL_COLUMN_ENABLED,
  XFAE_MODEL_COLUMN_REMOVABLE,
  XFAE_MODEL_COLUMN_TOOLTIP,
  XFAE_MODEL_RUN_HOOK,
  XFAE_MODEL_N_COLUMNS,
} XfaeModelColumn;

GType
xfae_model_get_type (void) G_GNUC_CONST;

GtkTreeModel *
xfae_model_new (void);

gboolean
xfae_model_add (XfaeModel *model,
                const gchar *name,
                const gchar *description,
                const gchar *command,
                XfsmRunHook run_hook,
                GError **error);

gboolean
xfae_model_get (XfaeModel *model,
                GtkTreeIter *iter,
                gchar **name,
                gchar **description,
                gchar **command,
                XfsmRunHook *run_hook,
                GError **error);

gboolean
xfae_model_remove (XfaeModel *model,
                   GtkTreeIter *iter,
                   GError **error);

gboolean
xfae_model_edit (XfaeModel *model,
                 GtkTreeIter *iter,
                 const gchar *name,
                 const gchar *description,
                 const gchar *command,
                 XfsmRunHook run_hook,
                 GError **error);

gboolean
xfae_model_toggle (XfaeModel *model,
                   GtkTreeIter *iter,
                   GError **error);

gboolean
xfae_model_set_run_hook (GtkTreeModel *tree_model,
                         GtkTreePath *path,
                         GtkTreeIter *iter,
                         XfsmRunHook run_hook,
                         GError **error);

G_END_DECLS;

#endif /* !__XFAE_MODEL_H__ */
