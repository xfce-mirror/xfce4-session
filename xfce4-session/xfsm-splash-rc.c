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

#include <xfce4-session/xfsm-splash-rc.h>


struct _XfsmSplashRc
{
  gchar  *group;
  XfceRc *rc;
};


XfsmSplashRc*
xfsm_splash_rc_new (XfceRc      *rc,
                    const gchar *group)
{
  XfsmSplashRc *splash_rc;

  splash_rc         = g_new (XfsmSplashRc, 1);
  splash_rc->group  = g_strdup (group);
  splash_rc->rc     = rc;

  return splash_rc;
}


const gchar*
xfsm_splash_rc_read_entry (XfsmSplashRc *splash_rc,
                           const gchar  *key,
                           const gchar  *fallback)
{
  xfce_rc_set_group (splash_rc->rc, splash_rc->group);
  return xfce_rc_read_entry (splash_rc->rc, key, fallback);
}


gint
xfsm_splash_rc_read_int_entry (XfsmSplashRc *splash_rc,
                               const gchar  *key,
                               gint          fallback)
{
  xfce_rc_set_group (splash_rc->rc, splash_rc->group);
  return xfce_rc_read_int_entry (splash_rc->rc, key, fallback);
}


gboolean
xfsm_splash_rc_read_bool_entry (XfsmSplashRc *splash_rc,
                                const gchar  *key,
                                gboolean      fallback)
{
  xfce_rc_set_group (splash_rc->rc, splash_rc->group);
  return xfce_rc_read_bool_entry (splash_rc->rc, key, fallback);
}


gchar**
xfsm_splash_rc_read_list_entry (XfsmSplashRc *splash_rc,
                                const gchar  *key,
                                const gchar  *delimiter)
{
  xfce_rc_set_group (splash_rc->rc, splash_rc->group);
  return xfce_rc_read_list_entry (splash_rc->rc, key, delimiter);
}


void
xfsm_splash_rc_write_entry (XfsmSplashRc *splash_rc,
                            const gchar  *key,
                            const gchar  *value)
{
  xfce_rc_set_group (splash_rc->rc, splash_rc->group);
  xfce_rc_write_entry (splash_rc->rc, key, value);
}


void
xfsm_splash_rc_write_int_entry (XfsmSplashRc *splash_rc,
                                const gchar  *key,
                                gint          value)
{
  xfce_rc_set_group (splash_rc->rc, splash_rc->group);
  xfce_rc_write_int_entry (splash_rc->rc, key, value);
}


void
xfsm_splash_rc_write_bool_entry (XfsmSplashRc *splash_rc,
                                 const gchar  *key,
                                 gboolean      value)
{
  xfce_rc_set_group (splash_rc->rc, splash_rc->group);
  xfce_rc_write_bool_entry (splash_rc->rc, key, value);
}


void
xfsm_splash_rc_write_list_entry (XfsmSplashRc *splash_rc,
                                 const gchar  *key,
                                 gchar       **value,
                                 const gchar  *delimiter)
{
  xfce_rc_set_group (splash_rc->rc, splash_rc->group);
  xfce_rc_write_list_entry (splash_rc->rc, key, value, delimiter);
}


void
xfsm_splash_rc_free (XfsmSplashRc *splash_rc)
{
  g_free (splash_rc->group);
  g_free (splash_rc);
}


