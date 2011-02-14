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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfsm/xfsm-splash-rc.h>

#define PROP_FROM_KEY(varname, key) \
  gchar varname[4096]; \
  g_strlcpy(varname, "/", sizeof(varname)); \
  g_strlcat(varname, key, sizeof(varname))


struct _XfsmSplashRc
{
  XfconfChannel *channel;
};


XfsmSplashRc*
xfsm_splash_rc_new (XfconfChannel *channel)
{
  XfsmSplashRc *splash_rc;

  splash_rc          = g_new (XfsmSplashRc, 1);
  splash_rc->channel = g_object_ref (channel);

  return splash_rc;
}


gchar*
xfsm_splash_rc_read_entry (XfsmSplashRc *splash_rc,
                           const gchar  *key,
                           const gchar  *fallback)
{
  PROP_FROM_KEY(prop, key);
  return xfconf_channel_get_string (splash_rc->channel, prop, fallback);
}


gint
xfsm_splash_rc_read_int_entry (XfsmSplashRc *splash_rc,
                               const gchar  *key,
                               gint          fallback)
{
  PROP_FROM_KEY(prop, key);
  return xfconf_channel_get_int (splash_rc->channel, prop, fallback);
}


gboolean
xfsm_splash_rc_read_bool_entry (XfsmSplashRc *splash_rc,
                                const gchar  *key,
                                gboolean      fallback)
{
  PROP_FROM_KEY(prop, key);
  return xfconf_channel_get_bool (splash_rc->channel, prop, fallback);
}


gchar**
xfsm_splash_rc_read_list_entry (XfsmSplashRc *splash_rc,
                                const gchar  *key,
                                const gchar  *delimiter)
{
  PROP_FROM_KEY(prop, key);
  return xfconf_channel_get_string_list (splash_rc->channel, prop);
}


void
xfsm_splash_rc_write_entry (XfsmSplashRc *splash_rc,
                            const gchar  *key,
                            const gchar  *value)
{
  PROP_FROM_KEY(prop, key);
  xfconf_channel_set_string (splash_rc->channel, prop, value);
}


void
xfsm_splash_rc_write_int_entry (XfsmSplashRc *splash_rc,
                                const gchar  *key,
                                gint          value)
{
  PROP_FROM_KEY(prop, key);
  xfconf_channel_set_int (splash_rc->channel, prop, value);
}


void
xfsm_splash_rc_write_bool_entry (XfsmSplashRc *splash_rc,
                                 const gchar  *key,
                                 gboolean      value)
{
  PROP_FROM_KEY(prop, key);
  xfconf_channel_set_bool (splash_rc->channel, prop, value);
}


void
xfsm_splash_rc_write_list_entry (XfsmSplashRc *splash_rc,
                                 const gchar  *key,
                                 gchar       **value,
                                 const gchar  *delimiter)
{
  PROP_FROM_KEY(prop, key);
  xfconf_channel_set_string_list (splash_rc->channel, prop, (gchar const **)value);
}


void
xfsm_splash_rc_free (XfsmSplashRc *splash_rc)
{
  g_object_unref (splash_rc->channel);
  g_free (splash_rc);
}


