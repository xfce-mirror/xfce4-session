/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
