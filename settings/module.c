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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gmodule.h>

#include <libxfce4ui/libxfce4ui.h>

#include <libxfsm/xfsm-splash-engine.h>
#include <libxfsm/xfsm-splash-rc.h>

#include "module.h"


struct _Module
{
  gchar             *engine;
  GModule           *handle;
  XfsmSplashConfig   config;
};



Module*
module_load (const gchar *path,
             const gchar *channel_name)
{
  void (*init) (XfsmSplashConfig *config);
  Module *module;
  gchar          property_base[512];
  XfconfChannel *channel;
  gchar         *dp;
  gchar         *sp;

  /* load module */
  module = g_new0 (Module, 1);
#if GLIB_CHECK_VERSION(2,4,0)
  module->handle = g_module_open (path, G_MODULE_BIND_LOCAL);
#else
  module->handle = g_module_open (path, 0);
#endif
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

  g_snprintf (property_base, sizeof (property_base),
              "/splash/engines/%s", module->engine);
  channel = xfconf_channel_new_with_property_base (channel_name,
                                                   property_base);

  /* initialize module */
  module->config.rc = xfsm_splash_rc_new (channel);
  g_object_unref (channel);
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
module_test (Module     *module,
             GdkDisplay *display)
{
  static char *steps[] =
  {
    "Starting the Window Manager",
    "Starting the Desktop Manager",
    "Starting the Taskbar",
    "Starting the Panel",
    NULL,
  };

  void (*init) (XfsmSplashEngine *engine);
  XfsmSplashEngine  engine;
  GdkScreen        *screen;
  guint             id;
  int               monitor;
  int               step;

  /* properly initialize the engine struct with zeros! */
  bzero (&engine, sizeof (engine));

  /* locate monitor with pointer */
  screen = xfce_gdk_screen_get_active (&monitor);
  if (G_UNLIKELY (screen == NULL) || (gdk_screen_get_display(screen) != display))
    {
      screen  = gdk_display_get_screen (display, 0);
      monitor = 0;
    }

  /* initialize engine struct */
  engine.display          = display;
  engine.primary_screen   = screen;
  engine.primary_monitor  = monitor;

  /* load and setup the engine */
  if (g_module_symbol (module->handle, "engine_init", (gpointer)&init))
    {
      init (&engine);

      if (G_LIKELY (engine.setup != NULL))
        {
          engine.setup (&engine, module->config.rc);
          gdk_flush ();
        }

      if (G_LIKELY (engine.start != NULL))
        {
          engine.start (&engine, "Default", NULL, 4);
          gdk_flush ();
        }

      if (G_LIKELY (engine.next != NULL))
        {
          for (step = 0; steps[step] != NULL; ++step)
            {
              engine.next (&engine, steps[step]);
              id = g_timeout_add (1000, (GSourceFunc) gtk_main_quit, NULL);
              gtk_main ();
              g_source_remove (id);
            }
        }

      if (G_LIKELY (engine.destroy != NULL))
        engine.destroy (&engine);
    }
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




