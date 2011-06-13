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

#ifndef __XFSM_PROPERTIES_H__
#define __XFSM_PROPERTIES_H__

#include <X11/SM/SMlib.h>

#include <libxfce4util/libxfce4util.h>

/* GNOME compatibility */
#define GsmPriority     "_GSM_Priority"
#define GsmDesktopFile  "_GSM_DesktopFile"

#define MAX_RESTART_ATTEMPTS 5

typedef struct _XfsmProperties XfsmProperties;

struct _XfsmProperties
{
  guint   restart_attempts;
  guint   restart_attempts_reset_id;

  guint   startup_timeout_id;

  GPid    pid;
  guint   child_watch_id;

  gchar  *client_id;
  gchar  *hostname;

  GTree  *sm_properties;
};


#define XFSM_PROPERTIES(p) ((XfsmProperties *) (p))


XfsmProperties *xfsm_properties_new (const gchar *client_id,
                                     const gchar *hostname) G_GNUC_PURE;
void            xfsm_properties_free    (XfsmProperties *properties);

void            xfsm_properties_extract (XfsmProperties *properties,
                                         gint           *num_props,
                                         SmProp       ***props);
void            xfsm_properties_store   (XfsmProperties *properties,
                                         XfceRc         *rc,
                                         const gchar    *prefix);

XfsmProperties* xfsm_properties_load (XfceRc *rc, const gchar *prefix);

gboolean xfsm_properties_check (const XfsmProperties *properties) G_GNUC_CONST;

const gchar *xfsm_properties_get_string (XfsmProperties *properties,
                                                  const gchar *property_name);
gchar **xfsm_properties_get_strv (XfsmProperties *properties,
                                  const gchar *property_name);
guchar xfsm_properties_get_uchar (XfsmProperties *properties,
                                  const gchar *property_name,
                                  guchar default_value);

const GValue *xfsm_properties_get (XfsmProperties *properties,
                                   const gchar *property_name);

void xfsm_properties_set_string (XfsmProperties *properties,
                                 const gchar *property_name,
                                 const gchar *property_value);
void xfsm_properties_set_strv (XfsmProperties *properties,
                               const gchar *property_name,
                               gchar **property_value);
void xfsm_properties_set_uchar (XfsmProperties *properties,
                                const gchar *property_name,
                                guchar property_value);

gboolean xfsm_properties_set (XfsmProperties *properties,
                              const gchar *property_name,
                              const GValue *property_value);
gboolean xfsm_properties_set_from_smprop (XfsmProperties *properties,
                                          const SmProp *sm_prop);

gboolean xfsm_properties_remove (XfsmProperties *properties,
                                 const gchar *property_name);

void xfsm_properties_set_default_child_watch (XfsmProperties *properties);

gint xfsm_properties_compare (const XfsmProperties *a,
                              const XfsmProperties *b) G_GNUC_CONST;

gint xfsm_properties_compare_id (const XfsmProperties *properties,
                                 const gchar *client_id);

#endif /* !__XFSM_PROPERTIES_H__ */
