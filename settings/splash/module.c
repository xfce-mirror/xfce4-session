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

#include <gmodule.h>

#include <xfce4-session/xfsm-splash-engine.h>
#include <xfce4-session/xfsm-splash-rc.h>

#include <settings/splash/module.h>


struct _Module
{
  gchar             *engine;
  GModule           *handle;
  XfsmSplashConfig   config;
};



Module*
module_load (const gchar *path,
             XfceRc      *rc)
{
  void (*init) (XfsmSplashConfig *config);
  Module *module;
  gchar   group[128];
  gchar  *dp;
  gchar  *sp;

  /* load module */
  module = g_new0 (Module, 1);
  module->handle = g_module_open (path, G_MODULE_BIND_LOCAL);
  if (G_UNLIKELY (module->handle == NULL))
    goto error0;
  if (!g_module_symbol (module->handle, "config_init", (gpointer)&init))
    goto error1;

  /* determine engine name */
  sp = module->engine = g_path_get_basename (path);
  if (sp[0] == 'l' && sp[1] == 'i' && sp[2] == 'b')
    sp += 3;
  for (dp = module->engine; *sp != '\0'; ++dp, ++sp)
    {
      if (*sp == '.')
        break;
      else
        *dp = *sp;
    }
  *dp = '\0';
  g_snprintf (group, 128, "Engine: %s", module->engine);

  /* initialize module */
  module->config.rc = xfsm_splash_rc_new (rc, group);
  init (&module->config);
  if (G_UNLIKELY (module->config.name == NULL))
    {
      module_free (module);
      return NULL;
    }

  /* succeed */
  return module;

error1:
  g_module_close (module->handle);
error0:
  g_free (module);
  return NULL;
}


const gchar*
module_engine (const Module *module)
{
  return module->engine;
}


const gchar*
module_name (const Module *module)
{
  return module->config.name;
}


const gchar*
module_descr (const Module *module)
{
  return module->config.description;
}


const gchar*
module_version (const Module *module)
{
  return module->config.version;
}


const gchar*
module_author (const Module *module)
{
  return module->config.author;
}


const gchar*
module_homepage (const Module *module)
{
  return module->config.homepage;
}


GdkPixbuf*
module_preview (Module *module)
{
  if (G_LIKELY (module->config.preview != NULL))
    return module->config.preview (&module->config);
  else
    return NULL;
}


gboolean
module_can_configure (const Module *module)
{
  return module->config.configure != NULL;
}


void
module_configure (Module    *module,
                  GtkWidget *parent)
{
  if (G_LIKELY (module->config.configure != NULL))
    module->config.configure (&module->config, parent);
}


void
module_free (Module *module)
{
  if (G_LIKELY (module->config.destroy != NULL))
    module->config.destroy (&module->config);

  if (G_LIKELY (module->config.name != NULL))
    g_free (module->config.name);

  if (G_LIKELY (module->config.description != NULL))
    g_free (module->config.description);

  if (G_LIKELY (module->config.version != NULL))
    g_free (module->config.version);

  if (G_LIKELY (module->config.author != NULL))
    g_free (module->config.author);

  if (G_LIKELY (module->config.homepage != NULL))
    g_free (module->config.homepage);

  xfsm_splash_rc_free (module->config.rc);
  g_module_close (module->handle);
  g_free (module->engine);
  g_free (module);
}




