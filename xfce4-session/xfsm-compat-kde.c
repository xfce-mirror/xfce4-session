/* $Id$ */
/*-
 * Copyright (c) 2004 Benedikt Meurer <benny@xfce.org>
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

#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/xfsm-compat-kde.h>


static void
run (const gchar *command)
{
  gchar buffer[2048];

  g_snprintf (buffer, 2048, "env DYLD_FORCE_FLAT_NAMESPACE= LD_BIND_NOW=true "
              "SESSION_MANAGER= %s", command);
  g_spawn_command_line_sync (buffer, NULL, NULL, NULL, NULL);
}


void
xfsm_compat_kde_startup (XfsmSplashScreen *splash)
{
  gchar command[256];

  xfsm_splash_screen_next (splash, _("Starting KDE services"));
  run ("kdeinit +kcminit +knotify");

  /* tell klauncher about the session manager */
  g_snprintf (command, 256, "dcop klauncher klauncher setLaunchEnv "
                            "SESSION_MANAGER \"%s\"",
                            g_getenv ("SESSION_MANAGER"));
  run (command);
}


void
xfsm_compat_kde_shutdown (void)
{
  /* shutdown KDE services */
  run ("kdeinit_shutdown");
  run ("dcopserver_shutdown");
  run ("artsshell -q terminate");
}

