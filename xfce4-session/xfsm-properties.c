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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
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

#include "libxfsm/xfsm-util.h"

#include "xfsm-global.h"
#include "xfsm-properties.h"


#ifdef ENABLE_X11
/* local prototypes */
static SmProp* strv_to_property (const gchar  *name,
                                 gchar       **argv)  G_GNUC_PURE;
static SmProp* str_to_property  (const gchar  *name,
                                 const gchar  *value) G_GNUC_PURE;
static SmProp* int_to_property  (const gchar  *name,
                                 gint          value) G_GNUC_PURE;
#endif

/* these three structs hold lists of properties that we save in
 * and load from the session file */
static const struct
{
  const gchar *name;
  const gchar *xsmp_name;
} strv_properties[] = {
  { "CloneCommand", SmCloneCommand },
  { "DiscardCommand", SmDiscardCommand },
  { "Environment", SmEnvironment },
  { "ResignCommand", SmResignCommand },
  { "RestartCommand", SmRestartCommand },
  { "ShutdownCommand", SmShutdownCommand },
  { NULL, NULL }
};

static const struct
{
  const gchar *name;
  const gchar *xsmp_name;
} str_properties[] = {
  { "CurrentDirectory", SmCurrentDirectory },
  { "DesktopFile", GsmDesktopFile },
  { "Program", SmProgram },
  { "UserId", SmUserID },
  { NULL, NULL }
};

static const struct
{
  const gchar *name;
  const gchar *xsmp_name;
  const guchar default_value;
} uchar_properties[] = {
  { "Priority", GsmPriority, 50 },
  { "RestartStyleHint", SmRestartStyleHint, SmRestartIfRunning },
  { NULL, NULL, 0 }
};


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


#ifdef ENABLE_X11
static SmProp*
strv_to_property (const gchar *name,
                  gchar      **argv)
{
  SmProp *prop;
  gint    argc;

  prop       = g_new (SmProp, 1);
  prop->name = g_strdup (name);
  prop->type = g_strdup (SmLISTofARRAY8);

  for (argc = 0; argv[argc] != NULL; ++argc)
    ;

  prop->num_vals = argc;
  prop->vals     = g_new (SmPropValue, argc);

  while (argc-- > 0)
    {
      prop->vals[argc].length = strlen (argv[argc]) + 1;
      prop->vals[argc].value  = g_strdup (argv[argc]);
    }

  return prop;
}


static SmProp*
str_to_property (const gchar *name,
                 const gchar *value)
{
  SmProp *prop;

  prop                 = g_new (SmProp, 1);
  prop->name           = g_strdup (name);
  prop->type           = g_strdup (SmARRAY8);
  prop->num_vals       = 1;
  prop->vals           = g_new (SmPropValue, 1);
  prop->vals[0].length = strlen (value) + 1;
  prop->vals[0].value  = g_strdup (value);

  return prop;
}


static SmProp*
int_to_property (const gchar *name,
                 gint         value)
{
  SmProp *prop;
  gint8  *p;

  p    = g_new (gint8, 1);
  p[0] = (gint8) value;

  prop                 = g_new (SmProp, 1);
  prop->name           = g_strdup (name);
  prop->type           = g_strdup (SmCARD8);
  prop->num_vals       = 1;
  prop->vals           = g_new (SmPropValue, 1);
  prop->vals[0].length = 1;
  prop->vals[0].value  = p;

  return prop;
}
#endif


XfsmProperties*
xfsm_properties_new (const gchar *client_id,
                     const gchar *hostname)
{
  XfsmProperties *properties;

  properties = g_slice_new0 (XfsmProperties);
  properties->client_id = g_strdup (client_id);
  properties->hostname  = g_strdup (hostname);
  properties->pid       = -1;

  properties->sm_properties = g_tree_new_full ((GCompareDataFunc) G_CALLBACK (strcmp),
                                               NULL,
                                               (GDestroyNotify) g_free,
                                               (GDestroyNotify) xfsm_g_value_free);

  return properties;
}


#ifdef ENABLE_X11
static gboolean
xfsm_properties_extract_foreach (gpointer key,
                                 gpointer value,
                                 gpointer data)
{
  const gchar  *prop_name = key;
  const GValue *prop_value = value;
  SmProp     ***pp = data;

  if (G_VALUE_HOLDS (prop_value, G_TYPE_STRV))
    **pp++ = strv_to_property (prop_name, g_value_get_boxed (prop_value));
  else if (G_VALUE_HOLDS_STRING (prop_value))
    **pp++ = str_to_property (prop_name, g_value_get_string (prop_value));
  else if (G_VALUE_HOLDS_UCHAR (prop_value))
    **pp++ = int_to_property (prop_name, g_value_get_uchar (prop_value));
  else {
    g_warning ("Unhandled property \"%s\" with type \"%s\"", prop_name,
               g_type_name (G_VALUE_TYPE (prop_value)));
  }

  return FALSE;
}

void
xfsm_properties_extract (XfsmProperties *properties,
                         gint           *num_props,
                         SmProp       ***props)
{
  SmProp **pp;

  g_return_if_fail (num_props != NULL);
  g_return_if_fail (props != NULL);

  *props = pp = g_new (SmProp *, g_tree_nnodes (properties->sm_properties));

  g_tree_foreach (properties->sm_properties,
                  xfsm_properties_extract_foreach,
                  &pp);

  *num_props = pp - *props;
}
#endif


XfsmProperties *
xfsm_properties_load (GKeyFile    *file,
                      const gchar *prefix,
                      const gchar *group)
{
#define ENTRY(name) (compose(buffer, 256, prefix, (name)))

  XfsmProperties *properties;
  const gchar    *client_id;
  const gchar    *hostname;
  GError         *error = NULL;
  const gchar    *value_str;
  gchar         **value_strv;
  gint            value_int;
  gchar           buffer[256];
  gint            i;

  client_id = g_key_file_get_string (file, group, ENTRY ("ClientId"), &error);
  if (client_id == NULL)
    {
      g_warning ("Session data broken, stored client is missing a client id. "
                 "Skipping client: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  hostname = g_key_file_get_string (file, group, ENTRY ("Hostname"), &error);
  if (hostname == NULL)
    {
      g_warning ("Session data broken, stored client is missing a hostname. "
                 "Skipping client: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  xfsm_verbose ("Loading properties for client %s\n", client_id);

  properties = xfsm_properties_new (client_id, hostname);

  for (i = 0; strv_properties[i].name; ++i)
    {
      value_strv = g_key_file_get_string_list (file, group, ENTRY (strv_properties[i].name), NULL, NULL);
      if (value_strv)
        {
          /* unescape delimiter in list entries */
          for (gchar **str = value_strv; *str != NULL; str++)
            {
              gchar *unesc_str = xfce_str_replace (*str, "\\" SESSION_FILE_DELIMITER, SESSION_FILE_DELIMITER);
              g_free (*str);
              *str = unesc_str;
            }

          xfsm_properties_set_strv (properties, strv_properties[i].xsmp_name, value_strv);
          g_strfreev (value_strv);
        }
    }

  for (i = 0; str_properties[i].name; ++i)
    {
      value_str = g_key_file_get_string (file, group, ENTRY (str_properties[i].name), NULL);
      if (value_str)
        xfsm_properties_set_string (properties, str_properties[i].xsmp_name, value_str);
    }

  for (i = 0; uchar_properties[i].name; ++i)
    {
      value_int = g_key_file_get_integer (file, group, ENTRY (uchar_properties[i].name), &error);
      if (error != NULL)
        {
          value_int = uchar_properties[i].default_value;
          g_clear_error (&error);
        }
      xfsm_properties_set_uchar (properties, uchar_properties[i].xsmp_name, value_int);
    }

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
                       GKeyFile       *file,
                       const gchar    *prefix,
                       const gchar    *group)
{
#define ENTRY(name) (compose(buffer, 256, prefix, (name)))

  GValue *value;
  gint    i;
  gchar   buffer[256];

  g_key_file_set_string (file, group, ENTRY ("ClientId"), properties->client_id);
  g_key_file_set_string (file, group, ENTRY ("Hostname"), properties->hostname);

  for (i = 0; strv_properties[i].name; ++i)
    {
      value = g_tree_lookup (properties->sm_properties, strv_properties[i].xsmp_name);
      if (value)
        {
          /* escape delimiter in list entries */
          const gchar * const *strv = g_value_get_boxed (value);
          guint len = g_strv_length ((gchar **) strv);
          gchar **esc_strv = g_new0 (gchar *, len + 1);
          for (guint j = 0; j < len; j++)
            esc_strv[j] = xfce_str_replace (strv[j], SESSION_FILE_DELIMITER, "\\" SESSION_FILE_DELIMITER);

          g_key_file_set_string_list (file, group, ENTRY (strv_properties[i].name),
                                      (const gchar * const *) esc_strv, len);
          g_strfreev (esc_strv);
        }
    }

  for (i = 0; str_properties[i].name; ++i)
    {
      value = g_tree_lookup (properties->sm_properties, str_properties[i].xsmp_name);
      if (value)
        {
          g_key_file_set_string (file, group, ENTRY (str_properties[i].name),
                                 g_value_get_string (value));
        }
    }

  for (i = 0; uchar_properties[i].name; ++i)
    {
      value = g_tree_lookup (properties->sm_properties, uchar_properties[i].xsmp_name);
      if (value)
        {
          g_key_file_set_integer (file, group, ENTRY (uchar_properties[i].name),
                                  g_value_get_uchar (value));
        }
    }

#undef ENTRY
}


gint
xfsm_properties_compare (const XfsmProperties *a,
                         const XfsmProperties *b)
{
  GValue *va, *vb;
  gint ia = 50, ib = 50;

  va = g_tree_lookup (a->sm_properties, GsmPriority);
  if (va)
    ia = g_value_get_uchar (va);

  vb = g_tree_lookup (b->sm_properties, GsmPriority);
  if (vb)
    ib = g_value_get_uchar (vb);

  return ia - ib;
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
    && g_tree_lookup (properties->sm_properties, SmProgram) != NULL
    && g_tree_lookup (properties->sm_properties, SmRestartCommand) != NULL;
}


const gchar *
xfsm_properties_get_string (XfsmProperties *properties,
                            const gchar *property_name)
{
  GValue *value;

  g_return_val_if_fail (properties != NULL, NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  value = g_tree_lookup (properties->sm_properties, property_name);

  if (G_LIKELY (value && G_VALUE_HOLDS_STRING (value)))
    return g_value_get_string (value);

  return NULL;
}


gchar **
xfsm_properties_get_strv (XfsmProperties *properties,
                          const gchar *property_name)
{
  GValue *value;

  g_return_val_if_fail (properties != NULL, NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  value = g_tree_lookup (properties->sm_properties, property_name);

  if (G_LIKELY (value && G_VALUE_HOLDS (value, G_TYPE_STRV)))
    return g_value_get_boxed (value);

  return NULL;
}


guchar
xfsm_properties_get_uchar (XfsmProperties *properties,
                           const gchar *property_name,
                           guchar default_value)
{
  GValue *value;

  g_return_val_if_fail (properties != NULL, default_value);
  g_return_val_if_fail (property_name != NULL, default_value);

  value = g_tree_lookup (properties->sm_properties, property_name);

  if (G_LIKELY (value && G_VALUE_HOLDS_UCHAR (value)))
    return g_value_get_uchar (value);

  return default_value;
}


const GValue *
xfsm_properties_get (XfsmProperties *properties,
                     const gchar *property_name)
{
  g_return_val_if_fail (properties != NULL, NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  return g_tree_lookup (properties->sm_properties, property_name);
}


void
xfsm_properties_set_string (XfsmProperties *properties,
                            const gchar *property_name,
                            const gchar *property_value)
{
  GValue *value;

  g_return_if_fail (properties != NULL);
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (property_value != NULL);

  xfsm_verbose ("-> Set string (%s, %s)\n", property_name, property_value);

  value = g_tree_lookup (properties->sm_properties, property_name);
  if (value)
    {
      if (!G_VALUE_HOLDS_STRING (value))
        {
          g_value_unset (value);
          g_value_init (value, G_TYPE_STRING);
        }
      g_value_set_string (value, property_value);
    }
  else
    {
      value = xfsm_g_value_new (G_TYPE_STRING);
      g_value_set_string (value, property_value);
      g_tree_replace (properties->sm_properties,
                      g_strdup (property_name),
                      value);
    }
}


void
xfsm_properties_set_strv (XfsmProperties *properties,
                          const gchar *property_name,
                          gchar **property_value)
{
  GValue *value;

  g_return_if_fail (properties != NULL);
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (property_value != NULL);

  xfsm_verbose ("-> Set strv (%s)\n", property_name);

  value = g_tree_lookup (properties->sm_properties, property_name);
  if (value)
    {
      if (!G_VALUE_HOLDS (value, G_TYPE_STRV))
        {
          g_value_unset (value);
          g_value_init (value, G_TYPE_STRV);
        }
      g_value_set_boxed (value, property_value);
    }
  else
    {
      value = xfsm_g_value_new (G_TYPE_STRV);
      g_value_set_boxed (value, property_value);
      g_tree_replace (properties->sm_properties,
                      g_strdup (property_name),
                      value);
    }
}

void
xfsm_properties_set_uchar (XfsmProperties *properties,
                           const gchar *property_name,
                           guchar property_value)
{
  GValue *value;

  g_return_if_fail (properties != NULL);
  g_return_if_fail (property_name != NULL);

  xfsm_verbose ("-> Set uchar (%s, %d)\n", property_name, property_value);

  value = g_tree_lookup (properties->sm_properties, property_name);
  if (value)
    {
      if (!G_VALUE_HOLDS_UCHAR (value))
        {
          g_value_unset (value);
          g_value_init (value, G_TYPE_UCHAR);
        }
      g_value_set_uchar (value, property_value);
    }
  else
    {
      value = xfsm_g_value_new (G_TYPE_UCHAR);
      g_value_set_uchar (value, property_value);
      g_tree_replace (properties->sm_properties,
                      g_strdup (property_name),
                      value);
    }
}


gboolean
xfsm_properties_set (XfsmProperties *properties,
                     const gchar *property_name,
                     const GValue *property_value)
{
  GValue *new_value;

  g_return_val_if_fail (properties != NULL, FALSE);
  g_return_val_if_fail (property_name != NULL, FALSE);
  g_return_val_if_fail (property_value != NULL, FALSE);

  if (!G_VALUE_HOLDS (property_value, G_TYPE_STRV)
      && !G_VALUE_HOLDS_STRING (property_value)
      && !G_VALUE_HOLDS_UCHAR (property_value))
    {
      g_warning ("Unhandled property \"%s\" of type \"%s\"", property_name,
                 g_type_name (G_VALUE_TYPE (property_value)));
      return FALSE;
    }

  xfsm_verbose ("-> Set (%s)\n", property_name);

  new_value = xfsm_g_value_new (G_VALUE_TYPE (property_value));
  g_value_copy (property_value, new_value);

  g_tree_replace (properties->sm_properties, g_strdup (property_name), new_value);

  return TRUE;
}

#ifdef ENABLE_X11
gboolean
xfsm_properties_set_from_smprop (XfsmProperties *properties,
                                 const SmProp *sm_prop)
{
  gchar **value_strv;
  guchar  value_uchar;
  gint    n;

  g_return_val_if_fail (properties != NULL, FALSE);
  g_return_val_if_fail (sm_prop != NULL, FALSE);

  if (!strcmp (sm_prop->type, SmLISTofARRAY8))
    {
      if (G_UNLIKELY (!sm_prop->num_vals || !sm_prop->vals))
        return FALSE;

      value_strv = g_new0 (gchar *, sm_prop->num_vals + 1);
      for (n = 0; n < sm_prop->num_vals; ++n)
        value_strv[n] = g_strdup ((const gchar *) sm_prop->vals[n].value);

      xfsm_properties_set_strv (properties, sm_prop->name, value_strv);
      g_strfreev (value_strv);
    }
  else if (!strcmp (sm_prop->type, SmARRAY8))
    {
      if (G_UNLIKELY (!sm_prop->vals[0].value))
        return FALSE;

      xfsm_properties_set_string (properties, sm_prop->name, sm_prop->vals[0].value);
    }
  else if (!strcmp (sm_prop->type, SmCARD8))
    {
      value_uchar = *(guchar *)(sm_prop->vals[0].value);
      xfsm_properties_set_uchar (properties, sm_prop->name, value_uchar);
    }
  else
    {
      g_warning ("Unhandled SMProp type: \"%s\"", sm_prop->type);
      return FALSE;
    }

  return TRUE;
}
#endif


gboolean
xfsm_properties_remove (XfsmProperties *properties,
                        const gchar *property_name)
{
  g_return_val_if_fail (properties != NULL, FALSE);
  g_return_val_if_fail (property_name != NULL, FALSE);

  xfsm_verbose ("-> Removing (%s)\n", property_name);

  return g_tree_remove (properties->sm_properties, property_name);
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
                         (GChildWatchFunc) G_CALLBACK (g_spawn_close_pid),
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

  g_tree_destroy (properties->sm_properties);

  g_slice_free (XfsmProperties, properties);
}
