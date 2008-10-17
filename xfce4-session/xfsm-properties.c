/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-properties.h>


/* local prototypes */
static SmProp* strv_to_property (const gchar  *name,
                                 gchar       **argv)  G_GNUC_PURE;
static SmProp* str_to_property  (const gchar  *name,
                                 const gchar  *value) G_GNUC_PURE;
static SmProp* int_to_property  (const gchar  *name,
                                 gint          value) G_GNUC_PURE;


#ifndef HAVE_STRDUP
static char*
strdup (const char *s)
{
  char *t;

  t = (char *) malloc (strlen (s) + 1);
  if (t != NULL)
    strcpy (t, s);

  return t;
}
#endif


static gchar*
compose (gchar       *buffer,
         gsize        length,
         const gchar *prefix,
         const gchar *suffix)
{
  g_strlcpy (buffer, prefix, length);
  g_strlcat (buffer, suffix, length);
  return buffer;
}


static SmProp*
strv_to_property (const gchar *name,
                  gchar      **argv)
{
  SmProp *prop;
  gint    argc;
  
  prop       = (SmProp *) malloc (sizeof (*prop));
  prop->name = strdup (name);
  prop->type = strdup (SmLISTofARRAY8);
  
  for (argc = 0; argv[argc] != NULL; ++argc)
    ;
  
  prop->num_vals = argc;
  prop->vals     = (SmPropValue *) malloc (argc * sizeof (SmPropValue));
  
  while (argc-- > 0)
    {
      prop->vals[argc].length = strlen (argv[argc]) + 1;
      prop->vals[argc].value  = strdup (argv[argc]);
    }

  return prop;
}


static SmProp*
str_to_property (const gchar *name,
                 const gchar *value)
{
  SmProp *prop;
  
  prop                 = (SmProp *) malloc (sizeof (*prop));
  prop->name           = strdup (name);
  prop->type           = strdup (SmARRAY8);
  prop->num_vals       = 1;
  prop->vals           = (SmPropValue *) malloc (sizeof (SmPropValue));
  prop->vals[0].length = strlen (value) + 1;
  prop->vals[0].value  = strdup (value);
  
  return prop;
}


static SmProp*
int_to_property (const gchar *name,
                 gint         value)
{
  SmProp *prop;
  gint8  *p;
  
  p    = (gint8 *) malloc (1);
  p[0] = (gint8) value;
  
  prop                 = (SmProp *) malloc (sizeof (*prop));
  prop->name           = strdup (name);
  prop->type           = strdup (SmCARD8);
  prop->num_vals       = 1;
  prop->vals           = (SmPropValue *) malloc (sizeof (SmPropValue));
  prop->vals[0].length = 1;
  prop->vals[0].value  = p;
  
  return prop;
}


XfsmProperties*
xfsm_properties_new (const gchar *client_id,
                     const gchar *hostname)
{
  XfsmProperties *properties;
  
  properties = g_new0 (XfsmProperties, 1);
  properties->client_id = g_strdup (client_id);
  properties->hostname  = g_strdup (hostname);
  properties->priority  = 50;
  properties->pid       = -1;
  
  return properties;
}


void
xfsm_properties_extract (XfsmProperties *properties,
                         gint           *num_props,
                         SmProp       ***props)
{
  SmProp **pp;
  
  g_return_if_fail (num_props != NULL);
  g_return_if_fail (props != NULL);
  
  *props = pp = (SmProp **) malloc (sizeof (SmProp *) * 20);
  
  if (properties->clone_command != NULL)
    *pp++ = strv_to_property (SmCloneCommand, properties->clone_command);
      
  if (properties->current_directory != NULL)
    *pp++ = str_to_property (SmCurrentDirectory, properties->current_directory);
  
  if (properties->discard_command != NULL)
    *pp++ = strv_to_property (SmDiscardCommand, properties->discard_command);

  if (properties->environment != NULL)
    *pp++ = strv_to_property (SmEnvironment, properties->environment);

  *pp++ = int_to_property (GsmPriority, properties->priority);

  if (properties->process_id != NULL)
    *pp++ = str_to_property (SmProcessID, properties->process_id);

  if (properties->program != NULL)
    *pp++ = str_to_property (SmProgram, properties->program);
  
  if (properties->resign_command != NULL)
    *pp++ = strv_to_property (SmResignCommand, properties->resign_command);

  if (properties->restart_command != NULL)
    *pp++ = strv_to_property (SmRestartCommand, properties->restart_command);
  
  *pp++ = int_to_property (SmRestartStyleHint, properties->restart_style_hint);
  
  if (properties->shutdown_command != NULL)
    *pp++ = strv_to_property (SmShutdownCommand, properties->shutdown_command);

  if (properties->user_id != NULL)
    *pp++ = str_to_property (SmUserID, properties->user_id);
  
  *num_props = pp - *props;
}


XfsmProperties*
xfsm_properties_load (XfceRc      *rc,
                      const gchar *prefix)
{
#define ENTRY(name) (compose(buffer, 256, prefix, (name)))

  XfsmProperties *properties;
  const gchar    *client_id;
  const gchar    *hostname;
  const gchar    *value;
  gchar           buffer[256];
  
  client_id = xfce_rc_read_entry (rc, ENTRY ("ClientId"), NULL);
  if (client_id == NULL)
    {
      g_warning ("Session data broken, stored client is missing a client id. "
                 "Skipping client.");
      return NULL;
    }

  hostname = xfce_rc_read_entry (rc, ENTRY ("Hostname"), NULL);
  if (hostname == NULL)
    {
      g_warning ("Session data broken, stored client is missing a hostname. "
                 "Skipping client.");
      return NULL;
    }
  
  properties                     = g_new0 (XfsmProperties, 1);
  properties->restart_attempts   = 0;
  properties->client_id          = g_strdup (client_id);
  properties->hostname           = g_strdup (hostname);
  properties->clone_command      = xfce_rc_read_list_entry (rc, ENTRY ("CloneCommand"),
                                                            NULL);
  properties->discard_command    = xfce_rc_read_list_entry (rc, ENTRY ("DiscardCommand"),
                                                            NULL);
  properties->environment        = xfce_rc_read_list_entry (rc, ENTRY ("Environment"),
                                                            NULL);
  properties->resign_command     = xfce_rc_read_list_entry (rc, ENTRY ("ResignCOmmand"),
                                                            NULL);
  properties->restart_command    = xfce_rc_read_list_entry (rc, ENTRY ("RestartCommand"),
                                                            NULL);
  properties->shutdown_command   = xfce_rc_read_list_entry (rc, ENTRY ("ShutdownCommand"),
                                                            NULL);
  properties->priority           = xfce_rc_read_int_entry (rc, ENTRY ("Priority"), 50);
  properties->restart_style_hint = xfce_rc_read_int_entry (rc, ENTRY ("RestartStyleHint"),
                                                           SmRestartIfRunning);
  
  value = xfce_rc_read_entry (rc, ENTRY ("CurrentDirectory"), NULL);
  if (value != NULL)
    properties->current_directory = g_strdup (value);
  
  value = xfce_rc_read_entry (rc, ENTRY ("Program"), NULL);
  if (value != NULL)
    properties->program = g_strdup (value);
  
  value = xfce_rc_read_entry (rc, ENTRY ("UserId"), NULL);
  if (value != NULL)
    properties->user_id = g_strdup (value);
  
  if (!xfsm_properties_check (properties))
    {
      xfsm_properties_free (properties);
      return NULL;
    }

  return properties;
  
#undef ENTRY
}


void
xfsm_properties_store (XfsmProperties *properties,
                       XfceRc         *rc,
                       const gchar    *prefix)
{
#define ENTRY(name) (compose(buffer, 256, prefix, (name)))

  gchar buffer[256];
  
  xfce_rc_write_entry (rc, ENTRY ("ClientId"), properties->client_id);
  xfce_rc_write_entry (rc, ENTRY ("Hostname"), properties->hostname);
  
  if (properties->clone_command != NULL)
    {
      xfce_rc_write_list_entry (rc, ENTRY ("CloneCommand"),
                                properties->clone_command, NULL);
    }
  
  if (properties->current_directory != NULL)
    {
      xfce_rc_write_entry (rc, ENTRY ("CurrentDirectory"),
                           properties->current_directory);
    }
  
  if (properties->discard_command != NULL)
    {
      xfce_rc_write_list_entry (rc, ENTRY ("DiscardCommand"),
                                properties->discard_command, NULL);
    }

  if (properties->environment != NULL)
    {
      xfce_rc_write_list_entry (rc, ENTRY ("Environment"),
                                properties->environment, NULL);
    }
  
  if (properties->priority != 50)
    {
      xfce_rc_write_int_entry (rc, ENTRY ("Priority"),
                               properties->priority);
    }

  /* ProcessID isn't something you'd generally want saved... */

  if (properties->program != NULL)
    {
      xfce_rc_write_entry (rc, ENTRY ("Program"),
                           properties->program);
    }

  if (properties->resign_command != NULL)
    {
      xfce_rc_write_list_entry (rc, ENTRY ("ResignCommand"),
                                properties->resign_command, NULL);
    }

  if (properties->restart_command != NULL)
    {
      xfce_rc_write_list_entry (rc, ENTRY ("RestartCommand"),
                                properties->restart_command, NULL);
    }

  if (properties->restart_style_hint != SmRestartIfRunning)
    {
      xfce_rc_write_int_entry (rc, ENTRY ("RestartStyleHint"),
                               properties->restart_style_hint);
    }

  if (properties->shutdown_command != NULL)
    {
      xfce_rc_write_list_entry (rc, ENTRY ("ShutdownCommand"),
                                properties->shutdown_command, NULL);
    }
  
  if (properties->user_id != NULL)
    {
      xfce_rc_write_entry (rc, ENTRY ("UserId"),
                           properties->user_id);
    }

#undef ENTRY
}


gint
xfsm_properties_compare (const XfsmProperties *a,
                         const XfsmProperties *b)
{
  return a->priority - b->priority;
}


gint
xfsm_properties_compare_id (const XfsmProperties *properties,
                            const gchar *client_id)
{
  return strcmp (properties->client_id, client_id);
}


gboolean
xfsm_properties_check (const XfsmProperties *properties)
{
  g_return_val_if_fail (properties != NULL, FALSE);
  
  return properties->client_id != NULL
    && properties->hostname != NULL
    && properties->program != NULL
    && properties->restart_command != NULL;
}


void
xfsm_properties_set_default_child_watch (XfsmProperties *properties)
{
  if (properties->child_watch_id > 0)
    {
      g_source_remove (properties->child_watch_id);
      properties->child_watch_id = 0;
    }

  if (properties->pid != -1)
    {
      /* if the PID is still open, we need to close it,
       * or it will become a zombie when it quits */
      g_child_watch_add (properties->pid,
                         (GChildWatchFunc) g_spawn_close_pid,
                         NULL);
      properties->pid = -1;
    }
}

void
xfsm_properties_free (XfsmProperties *properties)
{
  g_return_if_fail (properties != NULL);

  xfsm_properties_set_default_child_watch (properties);

  if (properties->restart_attempts_reset_id > 0)
    g_source_remove (properties->restart_attempts_reset_id);
  if (properties->startup_timeout_id > 0)
    g_source_remove (properties->startup_timeout_id);
  if (properties->client_id != NULL)
    g_free (properties->client_id);
  if (properties->hostname != NULL)
    g_free (properties->hostname);
  if (properties->clone_command != NULL)
    g_strfreev (properties->clone_command);
  if (properties->current_directory != NULL)
    g_free (properties->current_directory);
  if (properties->process_id != NULL)
    g_free (properties->process_id);
  if (properties->program != NULL)
    g_free (properties->program);
  if (properties->discard_command != NULL)
    g_strfreev (properties->discard_command);
  if (properties->resign_command != NULL)
    g_strfreev (properties->resign_command);
  if (properties->restart_command != NULL)
    g_strfreev (properties->restart_command);
  if (properties->shutdown_command != NULL)
    g_strfreev (properties->shutdown_command);
  if (properties->environment != NULL)
    g_strfreev (properties->environment);
  if (properties->user_id)
    g_free (properties->user_id);
  g_free (properties);
}


gchar **
xfsm_strv_from_smprop (const SmProp *prop)
{
  gchar **strv = NULL;
  gint    strc;
  guint   n;
  
  if (strcmp (prop->type, SmARRAY8) == 0)
    {
      if (!g_shell_parse_argv ((const gchar *) prop->vals->value,
                               &strc, &strv, NULL))
        return NULL;
    }
  else if (strcmp (prop->type, SmLISTofARRAY8) == 0)
    {
      strv = g_new (gchar *, prop->num_vals + 1);
      for (n = 0; n < prop->num_vals; ++n)
        strv[n] = g_strdup ((const gchar *) prop->vals[n].value);
      strv[n] = NULL;
    }

  return strv;
}


GValue *
xfsm_g_value_from_property (XfsmProperties *properties,
                            const gchar *name)
{
  GValue *val = NULL;

  if (strcmp (name, SmCloneCommand) == 0)
    {
      val = xfsm_g_value_new (G_TYPE_STRV);
      g_value_take_boxed (val, g_strdupv (properties->clone_command));
    }
  else if (strcmp (name, SmCurrentDirectory) == 0)
    {
      val = xfsm_g_value_new (G_TYPE_STRING);
      g_value_take_string (val, g_strdup (properties->current_directory));
    }
  else if (strcmp (name, SmDiscardCommand) == 0)
    {
      val = xfsm_g_value_new (G_TYPE_STRV);
      g_value_take_boxed (val, g_strdupv (properties->discard_command));
    }
  else if (strcmp (name, SmEnvironment) == 0)
    {
      val = xfsm_g_value_new (G_TYPE_STRV);
      g_value_take_boxed (val, g_strdupv (properties->environment));
    }
  else if (strcmp(name, SmProcessID) == 0)
    {
      val = xfsm_g_value_new (G_TYPE_STRING);
      g_value_take_string (val, g_strdup (properties->process_id));
    }
  else if (strcmp (name, SmProgram) == 0)
    {
      val = xfsm_g_value_new (G_TYPE_STRING);
      g_value_take_string (val, g_strdup (properties->program));
    }
  else if (strcmp (name, SmRestartCommand) == 0)
    {
      val = xfsm_g_value_new (G_TYPE_STRV);
      g_value_take_boxed (val, g_strdupv (properties->restart_command));
    }
  else if (strcmp (name, SmResignCommand) == 0)
    {
      val = xfsm_g_value_new (G_TYPE_STRV);
      g_value_take_boxed (val, g_strdupv (properties->resign_command));
    }
  else if (strcmp (name, SmRestartStyleHint) == 0)
    {
      val = xfsm_g_value_new (G_TYPE_UCHAR);
      g_value_set_uchar (val, properties->restart_style_hint);
    }
  else if (strcmp (name, SmShutdownCommand) == 0)
    {
      val = xfsm_g_value_new (G_TYPE_STRV);
      g_value_take_boxed (val, g_strdupv (properties->shutdown_command));
    }
  else if (strcmp (name, SmUserID) == 0)
    {
      val = xfsm_g_value_new (G_TYPE_STRING);
      g_value_take_string (val, g_strdup (properties->user_id));
    }
  else if (strcmp (name, GsmPriority) == 0)
    {
      val = xfsm_g_value_new (G_TYPE_UCHAR);
      g_value_set_uchar (val, properties->priority);
    }

  return val;
}
