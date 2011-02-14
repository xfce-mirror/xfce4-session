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

#include <gmodule.h>

#include <libxfsm/xfsm-splash-engine.h>

#include <engines/balou/balou.h>


#define DEFAULT_THEME   "Default"


G_MODULE_EXPORT void engine_init (XfsmSplashEngine *engine);


static void
engine_setup (XfsmSplashEngine *engine,
              XfsmSplashRc     *rc)
{
  gchar      *theme_name;
  BalouTheme *theme;

  theme_name = xfsm_splash_rc_read_entry (rc, "Theme", DEFAULT_THEME);
  theme = balou_theme_load (theme_name);
  g_free (theme_name);

  balou_init (BALOU (engine->user_data),
              engine->display,
              engine->primary_screen,
              engine->primary_monitor,
              theme);
}


static void
engine_next (XfsmSplashEngine *engine,
             const gchar      *text)
{
  Balou *balou = BALOU (engine->user_data);

  balou_fadeout (balou);
  balou_fadein (balou, text);
}


static int
engine_run (XfsmSplashEngine *engine,
            GtkWidget        *dialog)
{
  return balou_run (BALOU (engine->user_data), dialog);
}


static void
engine_destroy (XfsmSplashEngine *engine)
{
  Balou *balou = BALOU (engine->user_data);

  if (G_LIKELY (balou != NULL))
    {
      balou_destroy (balou);
      g_free (balou);
    }
}


G_MODULE_EXPORT void
engine_init (XfsmSplashEngine *engine)
{
  engine->user_data   = g_new0 (Balou, 1);
  engine->setup       = engine_setup;
  engine->next        = engine_next;
  engine->run         = engine_run;
  engine->destroy     = engine_destroy;
}


