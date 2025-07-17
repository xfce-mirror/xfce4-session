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

#include "xfsm-inhibition.h"

struct _XfsmInhibition
{
  GObject parent;

  gchar *app_id;
  gchar *reason;
  guint flags;
  guint toplevel_xid;
  guint cookie;
};

enum
{
  PROP_0,
  PROP_REASON,
  PROP_APP_ID,
  PROP_FLAGS,
  PROP_TOPLEVEL_XID,
  PROP_COOKIE,
};

G_DEFINE_TYPE (XfsmInhibition, xfsm_inhibition, G_TYPE_OBJECT)

static void
xfsm_inhibition_init (XfsmInhibition *inhibition)
{
}

static void
xfsm_inhibition_set_app_id (XfsmInhibition *inhibition,
                            const gchar *app_id)
{
  g_return_if_fail (XFSM_IS_INHIBITION (inhibition));

  g_free (inhibition->app_id);
  inhibition->app_id = g_strdup (app_id);

  g_object_notify (G_OBJECT (inhibition), "app-id");
}

static void
xfsm_inhibition_set_reason (XfsmInhibition *inhibition,
                            const gchar *reason)
{
  g_return_if_fail (XFSM_IS_INHIBITION (inhibition));

  g_free (inhibition->reason);
  inhibition->reason = g_strdup (reason);

  g_object_notify (G_OBJECT (inhibition), "reason");
}

static void
xfsm_inhibition_set_cookie (XfsmInhibition *inhibition,
                            guint cookie)
{
  g_return_if_fail (XFSM_IS_INHIBITION (inhibition));

  if (inhibition->cookie != cookie)
    {
      inhibition->cookie = cookie;
      g_object_notify (G_OBJECT (inhibition), "cookie");
    }
}

static void
xfsm_inhibition_set_flags (XfsmInhibition *inhibition,
                           XfsmInhibitonFlag flags)
{
  g_return_if_fail (XFSM_IS_INHIBITION (inhibition));

  if (inhibition->flags != flags)
    {
      inhibition->flags = flags;
      g_object_notify (G_OBJECT (inhibition), "flags");
    }
}

static void
xfsm_inhibition_set_toplevel_xid (XfsmInhibition *inhibition,
                                  guint xid)
{
  g_return_if_fail (XFSM_IS_INHIBITION (inhibition));

  if (inhibition->toplevel_xid != xid)
    {
      inhibition->toplevel_xid = xid;
      g_object_notify (G_OBJECT (inhibition), "toplevel-xid");
    }
}


const gchar *
xfsm_inhibition_get_app_id (XfsmInhibition *inhibition)
{
  g_return_val_if_fail (XFSM_IS_INHIBITION (inhibition), NULL);

  return inhibition->app_id;
}

const gchar *
xfsm_inhibition_get_reason (XfsmInhibition *inhibition)
{
  g_return_val_if_fail (XFSM_IS_INHIBITION (inhibition), NULL);

  return inhibition->reason;
}

XfsmInhibitonFlag
xfsm_inhibition_get_flags (XfsmInhibition *inhibition)
{
  g_return_val_if_fail (XFSM_IS_INHIBITION (inhibition), 0);

  return inhibition->flags;
}

guint
xfsm_inhibition_get_toplevel_xid (XfsmInhibition *inhibition)
{
  g_return_val_if_fail (XFSM_IS_INHIBITION (inhibition), 0);

  return inhibition->toplevel_xid;
}

guint
xfsm_inhibition_get_cookie (XfsmInhibition *inhibition)
{
  g_return_val_if_fail (XFSM_IS_INHIBITION (inhibition), 0);

  return inhibition->cookie;
}

guint *
xfsm_inhibition_peek_cookie (XfsmInhibition *inhibition)
{
  g_return_val_if_fail (XFSM_IS_INHIBITION (inhibition), NULL);

  return &inhibition->cookie;
}


static void
xfsm_inhibition_set_property (GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  XfsmInhibition *self;

  g_return_if_fail (XFSM_IS_INHIBITION (object));

  self = XFSM_INHIBITION (object);

  switch (prop_id)
    {
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
xfsm_inhibition_get_property (GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
  XfsmInhibition *self;

  g_return_if_fail (XFSM_IS_INHIBITION (object));

  self = XFSM_INHIBITION (object);

  switch (prop_id)
    {
    case PROP_APP_ID:
      g_value_set_string (value, self->app_id);
      break;
    case PROP_REASON:
      g_value_set_string (value, self->reason);
      break;
    case PROP_FLAGS:
      g_value_set_uint (value, self->flags);
      break;
    case PROP_COOKIE:
      g_value_set_uint (value, self->cookie);
      break;
    case PROP_TOPLEVEL_XID:
      g_value_set_uint (value, self->toplevel_xid);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
xfsm_inhibition_finalize (GObject *object)
{
  XfsmInhibition *inhibition;

  g_return_if_fail (XFSM_IS_INHIBITION (object));

  inhibition = XFSM_INHIBITION (object);

  g_free (inhibition->app_id);
  g_free (inhibition->reason);

  G_OBJECT_CLASS (xfsm_inhibition_parent_class)->finalize (object);
}

static void
xfsm_inhibition_class_init (XfsmInhibitionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = xfsm_inhibition_finalize;
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
xfsm_inhibition_new (const char *app_id,
                     guint toplevel_xid,
                     XfsmInhibitonFlag flags,
                     const char *reason)
{
  return g_object_new (XFSM_TYPE_INHIBITION,
                       "app-id", app_id,
                       "reason", reason,
                       "flags", flags,
                       "toplevel-xid", toplevel_xid,
                       "cookie", g_random_int_range (1, G_MAXINT32),
                       NULL);
}
