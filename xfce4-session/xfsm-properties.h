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

#ifndef __XFSM_PROPERTIES_H__
#define __XFSM_PROPERTIES_H__

#include <X11/SM/SMlib.h>

#include <libxfce4util/libxfce4util.h>

/* GNOME compatibility */
#define GsmPriority "_GSM_Priority"

#define MAX_RESTART_ATTEMPTS 5

typedef struct _XfsmProperties XfsmProperties;

struct _XfsmProperties
{
  guint   restart_attempts;
  
  gchar  *client_id;
  gchar  *hostname;

  gchar **clone_command;
  gchar  *current_directory;
  gchar **discard_command;
  gchar **environment;
  gint    priority;
  gchar  *program;
  gchar **restart_command;
  gint    restart_style_hint;
  gchar  *user_id;
};


#define XFSM_PROPERTIES(p) ((XfsmProperties *) (p))


XfsmProperties *xfsm_properties_new (const gchar *client_id,
                                     const gchar *hostname) G_GNUC_PURE;
void            xfsm_properties_free    (XfsmProperties *properties);

void            xfsm_properties_delete  (XfsmProperties *properties,
                                         gint            num_props,
                                         gchar         **prop_names);
void            xfsm_properties_extract (XfsmProperties *properties,
                                         gint           *num_props,
                                         SmProp       ***props);
void            xfsm_properties_merge   (XfsmProperties *properties,
                                         gint            num_props,
                                         SmProp        **props);
void            xfsm_properties_store   (XfsmProperties *properties,
                                      XfceRc         *rc,
                                      const gchar    *prefix);

XfsmProperties* xfsm_properties_load (XfceRc *rc, const gchar *prefix);

gboolean xfsm_properties_check (const XfsmProperties *properties) G_GNUC_CONST;

gint xfsm_properties_compare (const XfsmProperties *a,
                              const XfsmProperties *b) G_GNUC_CONST;

#endif /* !__XFSM_PROPERTIES_H__ */
