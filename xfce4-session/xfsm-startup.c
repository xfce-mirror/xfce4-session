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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-manager.h>
#include <xfce4-session/xfsm-splash-screen.h>
#include <xfce4-session/xfsm-util.h>


/*
   Prototypes
 */
static gboolean xfsm_startup_continue_failsafe (void);
static gboolean xfsm_startup_continue_session  (const gchar *previous_id);


/* 
   Globals
 */
static gchar     *splash_theme_name = NULL;


void
xfsm_startup_init (XfceRc *rc)
{
  const gchar *name;

  xfce_rc_set_group (rc, "Splash Theme");
  name = xfce_rc_read_entry (rc, "Name", NULL);
  if (name != NULL)
    splash_theme_name = g_strdup (name);
  else
    splash_theme_name = NULL;
}


static gboolean
destroy_splash (gpointer user_data)
{
  if (splash_screen != NULL) {
    xfsm_splash_screen_destroy (splash_screen);
    splash_screen = NULL;
  }

  return FALSE;
}


static const gchar*
figure_app_name (const gchar *program_path)
{
  static char progbuf[256];
  gchar *prog;

  prog = g_path_get_basename (program_path);

  /* Xfce applications */
  if (strcmp (prog, "xfce4-mixer") == 0)
    return _("Starting the Volume Controller");
  else if (strcmp (prog, "xfce4-panel") == 0)
    return _("Starting the Panel");
  else if (strcmp (prog, "xfdesktop") == 0)
    return _("Starting the Desktop Manager");
  else if (strcmp (prog, "xftasbar4") == 0)
    return _("Starting the Taskbar");
  else if (strcmp (prog, "xfwm4") == 0)
    return _("Starting the Window Manager");

  /* Gnome applications */
  if (strcmp (prog, "gnome-terminal") == 0)
    return _("Starting the Gnome Terminal Emulator");

  /* 3rd party applications */
  if (strcmp (prog, "beep-media-player") == 0)
    return _("Starting the Beep Media Player");
  else if (strncmp (prog, "gimp", 4) == 0)
    return _("Starting The Gimp");
  else if (strcmp (prog, "gvim") == 0)
    return _("Starting the VI Improved Editor");
  else if (strcmp (prog, "smproxy") == 0)
    return _("Starting the Session Management Proxy");
  else if (strcmp (prog, "xchat") == 0 || strcmp (prog, "xchat2") == 0)
    return _("Starting the X-Chat IRC Client");
  else if (strcmp (prog, "xmms") == 0)
    return _("Starting the X Multimedia System");
  else if (strcmp (prog, "xterm") == 0)
    return _("Starting the X Terminal Emulator");

  g_snprintf (progbuf, 256, _("Starting %s"), prog);

  return progbuf;
}


static void
xfsm_startup_autostart (void)
{
  const gchar *entry;
  gchar *argv[] = { NULL, NULL };
  GError *err;
  gchar *file;
  gchar *dir;
  GDir *dirp;
  gint n = 0;

  dir = xfce_get_homefile ("Desktop", "Autostart", NULL);
  dirp = g_dir_open (dir, 0, NULL);
  if (dirp != NULL)
    {
      xfsm_splash_screen_next (splash_screen, _("Performing Autostart..."));

      for (;;)
        {
          entry = g_dir_read_name (dirp);
          if (entry == NULL)
            break;

          file = g_build_filename (dir, entry, NULL);

          if (g_file_test (file, G_FILE_TEST_IS_EXECUTABLE))
            {
              *argv = file;
              err = NULL;

              if (!g_spawn_async (NULL, argv, NULL, 0, NULL, NULL, NULL, &err))
                {
                  g_warning ("Unable to launch %s: %s", file, err->message);
                  g_error_free (err);
                }
              else
                {
                  ++n;
                }
            }

          g_free (file);
        }

      g_dir_close (dirp);
    }

  if (n > 0)
    g_timeout_add (2000, destroy_splash, NULL);
  else
    g_timeout_add (1000, destroy_splash, NULL);

  g_free (dir);
}


/* Returns TRUE if done, else FALSE */
gboolean
xfsm_startup_continue (const gchar *previous_id)
{
  gboolean startup_done = FALSE;

  if (failsafe_mode)
    startup_done = xfsm_startup_continue_failsafe ();
  else
    startup_done = xfsm_startup_continue_session (previous_id);

  /* perform Autostart */
  if (startup_done)
    {
      xfsm_startup_autostart ();
    }
  
  return startup_done;
}


static gboolean
xfsm_startup_continue_failsafe (void)
{  
  FailsafeClient *fclient;
  
  fclient = (FailsafeClient *) g_list_nth_data (failsafe_clients, 0);
  if (fclient != NULL)
    {
      /* let the user know whats going on */
      xfsm_splash_screen_next (splash_screen,
                               figure_app_name (fclient->command[0]));

      /* start the application */
      xfsm_start_application (fclient->command, NULL, fclient->screen,
                              NULL, NULL, NULL);
      failsafe_clients = g_list_remove (failsafe_clients, fclient);
      g_strfreev (fclient->command);
      g_free (fclient);

      /* there are more to come */
      return FALSE;
    }

  return TRUE;
}


static gboolean
xfsm_startup_continue_session (const gchar *previous_id)
{
  XfsmProperties *properties;
  GList *lp;
  
  properties = (XfsmProperties *) g_list_nth_data (pending_properties, 0);
  if (properties != NULL)
    {
      xfsm_splash_screen_next (splash_screen,
                               figure_app_name (properties->program));

      /* restart the application */
      xfsm_start_application (properties->restart_command, NULL, NULL,
                              NULL, NULL, NULL);
      pending_properties = g_list_remove (pending_properties, properties);
      starting_properties = g_list_append (starting_properties, properties);

      /* more to come... */
      return FALSE;
    }

  /* start up done, free all properties that failed to restart correctly */
  for (lp = starting_properties; lp != NULL; lp = lp->next)
    xfsm_properties_free (XFSM_PROPERTIES (lp->data));
  g_list_free (starting_properties);
  starting_properties = NULL;
  
  return TRUE;
}

