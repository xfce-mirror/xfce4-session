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


/* Returns TRUE if done, else FALSE */
gboolean
xfsm_startup_continue (const gchar *previous_id)
{
  gboolean startup_done = FALSE;

  if (failsafe_mode)
    startup_done = xfsm_startup_continue_failsafe ();
  else
    startup_done = xfsm_startup_continue_session (previous_id);

  /* get rid of the splash screen once finished */
  if (startup_done)
    g_timeout_add (1500, destroy_splash, NULL);
  
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
  
  properties = (XfsmProperties *) g_list_nth_data (pending_properties, 0);
  if (properties != NULL)
    {
      xfsm_splash_screen_next (splash_screen,
                               figure_app_name (properties->program));

      /* restart the application */
      xfsm_start_application (properties->restart_command, NULL, NULL,
                              NULL, NULL, NULL);
      pending_properties = g_list_remove (pending_properties, properties);
      xfsm_properties_free (properties);

      /* more to come... */
      return FALSE;
    }
  
  return TRUE;
}

