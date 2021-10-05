/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2021 Matias De lellis <mati86dl@gmail.com>
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

#include "config.h"

#include <glib.h>

#include "xfsm-inhibitor.h"

#define XFSM_INHIBITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), XFSM_TYPE_INHIBITOR, XfsmInhibitorPrivate))

struct XfsmInhibitorPrivate
{
        GHashTable *inhibitions;
};


static void     xfsm_inhibitor_finalize      (GObject             *object);

G_DEFINE_TYPE (XfsmInhibitor, xfsm_inhibitor, G_TYPE_OBJECT)


static gboolean
g_hash_func_inhibition_has_flag (gpointer key,
                                 gpointer value,
                                 gpointer data)
{
  guint flags = xfsm_inhibition_get_flags (XFSM_INHIBITION(value));
  guint flag = GPOINTER_TO_UINT (data);
  return (flags & flag);
}

static gboolean
g_hash_func_inhibition_cookie_cmp (gpointer key,
                                   gpointer value,
                                   gpointer data)
{
  guint cookie = xfsm_inhibition_get_cookie (XFSM_INHIBITION(value));
  return (cookie == GPOINTER_TO_UINT (data));
}


gboolean
xfsm_inhibitor_add (XfsmInhibitor  *store,
                    XfsmInhibition *inhibition)
{
  g_return_val_if_fail (store != NULL, FALSE);
  g_return_val_if_fail (inhibition != NULL, FALSE);

  g_debug ("XfsmInhibitor: Adding inhibitions %u to store",
           xfsm_inhibition_get_cookie (inhibition));
  g_hash_table_insert (store->priv->inhibitions,
                       xfsm_inhibition_peek_cookie (inhibition),
                       g_object_ref (inhibition));
  return TRUE;
}

gboolean
xfsm_inhibitor_remove (XfsmInhibitor *store,
                       guint          cookie)
{
  XfsmInhibition *inhibition = NULL;
  g_return_val_if_fail (store != NULL, FALSE);

  g_debug ("XfsmInhibitor: Removing inhibitions %u to store", cookie);

  inhibition = g_hash_table_find (store->priv->inhibitions,
                                  g_hash_func_inhibition_cookie_cmp,
                                  GUINT_TO_POINTER(cookie));
  if (inhibition == NULL)
    return FALSE;

  return g_hash_table_remove (store->priv->inhibitions,
                              xfsm_inhibition_peek_cookie (inhibition));
}

gboolean
xfsm_inhibitor_is_empty (XfsmInhibitor *store)
{
  g_return_val_if_fail (store != NULL, TRUE);
  return (g_hash_table_size (store->priv->inhibitions) == 0);
}

gboolean
xfsm_inhibitor_has_flags (XfsmInhibitor *store,
                          guint          flags)
{
  XfsmInhibition *inhibition = NULL;
  g_return_val_if_fail (store != NULL, FALSE);
  inhibition = g_hash_table_find (store->priv->inhibitions,
                                  g_hash_func_inhibition_has_flag,
                                  GUINT_TO_POINTER(flags));
  return (inhibition != NULL);
}

static void
xfsm_inhibitor_class_init (XfsmInhibitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xfsm_inhibitor_finalize;

  g_type_class_add_private (klass, sizeof (XfsmInhibitorPrivate));
}

static void
xfsm_inhibitor_init (XfsmInhibitor *store)
{
  store->priv = XFSM_INHIBITOR_GET_PRIVATE (store);

  store->priv->inhibitions = g_hash_table_new_full (g_int_hash,
                                                    g_int_equal,
                                                    NULL,
                                                    (GDestroyNotify) g_object_unref);
}

static void
xfsm_inhibitor_finalize (GObject *object)
{
  XfsmInhibitor *store;

  g_return_if_fail (object != NULL);
  g_return_if_fail (XFSM_IS_INHIBITOR (object));

  store = XFSM_INHIBITOR (object);

  g_return_if_fail (store->priv != NULL);

  g_hash_table_destroy (store->priv->inhibitions);

  G_OBJECT_CLASS (xfsm_inhibitor_parent_class)->finalize (object);
}

XfsmInhibitor *
xfsm_inhibitor_get (void)
{
  static XfsmInhibitor *object = NULL;

  if (G_LIKELY (object != NULL))
    {
      g_object_ref (G_OBJECT (object));
    }
  else
    {
      object = g_object_new (XFSM_TYPE_INHIBITOR, NULL);
      g_object_add_weak_pointer (G_OBJECT (object), (gpointer) &object);
    }

  return object;
}
