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


#include "xfsm-inhibition.h"

struct _XfsmInhibitionPrivate
{
  gchar *app_id;
  gchar *reason;
  guint  flags;
  guint  toplevel_xid;
  guint  cookie;
};

enum {
  PROP_0,
  PROP_REASON,
  PROP_APP_ID,
  PROP_FLAGS,
  PROP_TOPLEVEL_XID,
  PROP_COOKIE,
};

G_DEFINE_TYPE_WITH_PRIVATE (XfsmInhibition, xfsm_inhibition, G_TYPE_OBJECT)

static void
xfsm_inhibition_init (XfsmInhibition *inhibition)
{
  inhibition->priv = xfsm_inhibition_get_instance_private (inhibition);
}

static void
xfsm_inhibition_set_app_id (XfsmInhibition  *inhibition,
                            const gchar     *app_id)
{
  XfsmInhibitionPrivate *priv = NULL;

  g_return_if_fail (XFSM_IS_INHIBITION (inhibition));

  priv = xfsm_inhibition_get_instance_private (inhibition);

  g_free (priv->app_id);
  priv->app_id = g_strdup (app_id);

  g_object_notify (G_OBJECT (inhibition), "app-id");
}

static void
xfsm_inhibition_set_reason (XfsmInhibition  *inhibition,
                            const gchar     *reason)
{
  XfsmInhibitionPrivate *priv = NULL;

  g_return_if_fail (XFSM_IS_INHIBITION (inhibition));

  priv = xfsm_inhibition_get_instance_private (inhibition);

  g_free (priv->reason);
  priv->reason = g_strdup (reason);

  g_object_notify (G_OBJECT (inhibition), "reason");
}

static void
xfsm_inhibition_set_cookie (XfsmInhibition  *inhibition,
                            guint            cookie)
{
  XfsmInhibitionPrivate *priv = NULL;

  g_return_if_fail (XFSM_IS_INHIBITION (inhibition));

  priv = xfsm_inhibition_get_instance_private (inhibition);

  if (priv->cookie != cookie)
    {
      priv->cookie = cookie;
      g_object_notify (G_OBJECT (inhibition), "cookie");
    }
}

static void
xfsm_inhibition_set_flags (XfsmInhibition  *inhibition,
                           guint            flags)
{
  XfsmInhibitionPrivate *priv = NULL;

  g_return_if_fail (XFSM_IS_INHIBITION (inhibition));

  priv = xfsm_inhibition_get_instance_private (inhibition);

  if (priv->flags != flags)
    {
      priv->flags = flags;
      g_object_notify (G_OBJECT (inhibition), "flags");
    }
}

static void
xfsm_inhibition_set_toplevel_xid (XfsmInhibition  *inhibition,
                                  guint            xid)
{
  XfsmInhibitionPrivate *priv = NULL;

  g_return_if_fail (XFSM_IS_INHIBITION (inhibition));

  priv = xfsm_inhibition_get_instance_private (inhibition);

  if (priv->toplevel_xid != xid)
    {
      priv->toplevel_xid = xid;
      g_object_notify (G_OBJECT (inhibition), "toplevel-xid");
    }
}


const gchar *
xfsm_inhibition_get_app_id (XfsmInhibition  *inhibition)
{
  XfsmInhibitionPrivate *priv = NULL;

  g_return_val_if_fail (XFSM_IS_INHIBITION (inhibition), NULL);

  priv = xfsm_inhibition_get_instance_private (inhibition);

  return priv->app_id;
}

const gchar *
xfsm_inhibition_get_reason (XfsmInhibition  *inhibition)
{
  XfsmInhibitionPrivate *priv = NULL;

  g_return_val_if_fail (XFSM_IS_INHIBITION (inhibition), NULL);

  priv = xfsm_inhibition_get_instance_private (inhibition);

  return priv->reason;
}

guint
xfsm_inhibition_get_flags (XfsmInhibition  *inhibition)
{
  XfsmInhibitionPrivate *priv = NULL;

  g_return_val_if_fail (XFSM_IS_INHIBITION (inhibition), 0);

  priv = xfsm_inhibition_get_instance_private (inhibition);

  return priv->flags;
}

guint
xfsm_inhibition_get_toplevel_xid (XfsmInhibition  *inhibition)
{
  XfsmInhibitionPrivate *priv = NULL;

  g_return_val_if_fail (XFSM_IS_INHIBITION (inhibition), 0);

  priv = xfsm_inhibition_get_instance_private (inhibition);

  return priv->toplevel_xid;
}

guint
xfsm_inhibition_get_cookie (XfsmInhibition  *inhibition)
{
  XfsmInhibitionPrivate *priv = NULL;

  g_return_val_if_fail (XFSM_IS_INHIBITION (inhibition), 0);

  priv = xfsm_inhibition_get_instance_private (inhibition);

  return priv->cookie;
}

guint *
xfsm_inhibition_peek_cookie (XfsmInhibition  *inhibition)
{
  XfsmInhibitionPrivate *priv = NULL;

  g_return_val_if_fail (XFSM_IS_INHIBITION (inhibition), NULL);

  priv = xfsm_inhibition_get_instance_private (inhibition);

  return &priv->cookie;
}


static void
xfsm_inhibition_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  XfsmInhibition *self;

  self = XFSM_INHIBITION (object);

  switch (prop_id) {
  case PROP_APP_ID:
    xfsm_inhibition_set_app_id (self, g_value_get_string (value));
    break;
  case PROP_REASON:
    xfsm_inhibition_set_reason (self, g_value_get_string (value));
    break;
  case PROP_FLAGS:
    xfsm_inhibition_set_flags (self, g_value_get_uint (value));
    break;
  case PROP_COOKIE:
    xfsm_inhibition_set_cookie (self, g_value_get_uint (value));
    break;
  case PROP_TOPLEVEL_XID:
    xfsm_inhibition_set_toplevel_xid (self, g_value_get_uint (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
xfsm_inhibition_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  XfsmInhibition *self;
  XfsmInhibitionPrivate *priv = NULL;

  g_return_if_fail (XFSM_IS_INHIBITION (object));

  self = XFSM_INHIBITION (object);

  priv = xfsm_inhibition_get_instance_private (self);

  switch (prop_id) {
  case PROP_APP_ID:
    g_value_set_string (value, priv->app_id);
    break;
  case PROP_REASON:
    g_value_set_string (value, priv->reason);
    break;
  case PROP_FLAGS:
    g_value_set_uint (value, priv->flags);
    break;
  case PROP_COOKIE:
    g_value_set_uint (value, priv->cookie);
    break;
  case PROP_TOPLEVEL_XID:
    g_value_set_uint (value, priv->toplevel_xid);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
xfsm_inhibition_finalize (GObject *object)
{
  XfsmInhibition *inhibition = NULL;
  XfsmInhibitionPrivate *priv = NULL;

  g_return_if_fail (object != NULL);
  g_return_if_fail (XFSM_IS_INHIBITION (object));

  inhibition = XFSM_INHIBITION (object);
  priv = xfsm_inhibition_get_instance_private (inhibition);

  g_free (priv->app_id);
  g_free (priv->reason);

  G_OBJECT_CLASS (xfsm_inhibition_parent_class)->finalize (object);
}

static void
xfsm_inhibition_class_init (XfsmInhibitionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize     = xfsm_inhibition_finalize;
  object_class->get_property = xfsm_inhibition_get_property;
  object_class->set_property = xfsm_inhibition_set_property;

  g_object_class_install_property (object_class,
                                   PROP_APP_ID,
                                   g_param_spec_string ("app-id",
                                                        "app-id",
                                                        "app-id",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_REASON,
                                   g_param_spec_string ("reason",
                                                        "reason",
                                                        "reason",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_FLAGS,
                                   g_param_spec_uint ("flags",
                                                      "flags",
                                                      "flags",
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_TOPLEVEL_XID,
                                   g_param_spec_uint ("toplevel-xid",
                                                      "toplevel-xid",
                                                      "toplevel-xid",
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_COOKIE,
                                   g_param_spec_uint ("cookie",
                                                      "cookie",
                                                      "cookie",
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

XfsmInhibition *
xfsm_inhibition_new (const char    *app_id,
                     guint          toplevel_xid,
                     guint          flags,
                     const char    *reason)
{
  XfsmInhibition *inhibition;
  inhibition = g_object_new (XFSM_TYPE_INHIBITION,
                             "app-id", app_id,
                             "reason", reason,
                             "flags", flags,
                             "toplevel-xid", toplevel_xid,
                             "cookie", g_random_int_range (1, G_MAXINT32),
                             NULL);
  return inhibition;
}
