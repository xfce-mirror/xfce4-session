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
 *
 * The session id generator was taken from the KDE session manager.
 * Copyright (c) 2000 Matthias Ettrich <ettrich@kde.org>
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

#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>

#include <gtk/gtk.h>

#include <libxfcegui4/libxfcegui4.h>

#include <xfce4-session/shutdown.h>
#include <xfce4-session/xfsm-chooser.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-legacy.h>
#include <xfce4-session/xfsm-manager.h>
#include <xfce4-session/xfsm-startup.h>
#include <xfce4-session/xfsm-util.h>


#define DEFAULT_SESSION_NAME "Default"


/*
   Prototypes
 */
static gboolean xfsm_manager_startup (void);
static void     xfsm_manager_handle_failed (void);
static void     xfsm_manager_startup_continue (const gchar *previous_id);
static gboolean xfsm_manager_startup_timedout (gpointer user_data);
static void     xfsm_manager_load_settings (XfceRc *rc);
static gboolean xfsm_manager_load_session (void);


/*
   Static data
 */
static XfsmManagerState state = XFSM_MANAGER_STARTUP;
static guint            die_timeout_id = 0;
static guint            startup_timeout_id = 0;


static gboolean
xfsm_manager_startup (void)
{
  xfsm_startup_foreign ();
  pending_properties = g_list_sort (pending_properties,
                                    (GCompareFunc) xfsm_properties_compare);
  xfsm_manager_startup_continue (NULL);
  return FALSE;
}


static void
xfsm_manager_restore_active_workspace (XfceRc *rc)
{
  NetkWorkspace  *workspace;
  GdkDisplay     *display;
  NetkScreen     *screen;
  gchar           buffer[1024];
  gint            n, m;

  display = gdk_display_get_default ();
  for (n = 0; n < gdk_display_get_n_screens (display); ++n)
    {
      g_snprintf (buffer, 1024, "Screen%d_ActiveWorkspace", n);
      if (!xfce_rc_has_entry (rc, buffer))
        continue;
      m = xfce_rc_read_int_entry (rc, buffer, 0);

      screen = netk_screen_get (n);
      netk_screen_force_update (screen);

      if (netk_screen_get_workspace_count (screen) > m)
        {
          workspace = netk_screen_get_workspace (screen, m);
          netk_workspace_activate (workspace);
        }
    }
}


static void
xfsm_manager_handle_failed (void)
{
  /* Handle apps that failed to start here */
  XfsmProperties *properties;
  GList          *lp;

  for (lp = starting_properties; lp != NULL; lp = lp->next)
    {
      properties = XFSM_PROPERTIES (lp->data);

      xfsm_verbose ("Client Id = %s failed to start, running discard "
                    "command now.\n\n", properties->client_id);

      g_spawn_sync (properties->current_directory,
                    properties->discard_command,
                    properties->environment,
                    G_SPAWN_SEARCH_PATH,
                    NULL, NULL,
                    NULL, NULL,
                    NULL, NULL);

      xfsm_properties_free (properties);
    }

  g_list_free (starting_properties);
  starting_properties = NULL;
}


static void
xfsm_manager_startup_continue (const gchar *previous_id)
{
  gboolean startup_done = FALSE;
  gchar buffer[1024];
  XfceRc *rc;

  xfsm_verbose ("Manager startup continues [Previous Id = %s]\n\n",
                previous_id != NULL ? previous_id : "None");

  if (startup_timeout_id != 0)
    {
      g_source_remove (startup_timeout_id);
      startup_timeout_id = 0;

      /* work around broken clients */
      if (state != XFSM_MANAGER_STARTUP)
        return;
    }

  startup_done = xfsm_startup_continue (previous_id);

  if (startup_done)
    {
      xfsm_verbose ("Manager finished startup, entering IDLE mode now\n\n");
      state = XFSM_MANAGER_IDLE;

      if (!failsafe_mode)
        {
          /* handle apps that failed to start */
          xfsm_manager_handle_failed ();

          /* restore active workspace, this has to be done after the
           * window manager is up, so we do it last.
           */
          g_snprintf (buffer, 1024, "Session: %s", session_name);
          rc = xfce_rc_simple_open (session_file, TRUE);
          xfce_rc_set_group (rc, buffer);
          xfsm_manager_restore_active_workspace (rc);
          xfce_rc_close (rc);

          /* start legacy applications now */
          xfsm_legacy_startup ();
        }
    }
  else
    {
      startup_timeout_id = g_timeout_add (STARTUP_TIMEOUT,
                                          xfsm_manager_startup_timedout,
                                          NULL);
    }
}


static gboolean
xfsm_manager_startup_timedout (gpointer user_data)
{
  xfsm_verbose ("Manager startup timed out\n\n");

  startup_timeout_id = 0; /* will be removed automagically once we return */
  xfsm_manager_startup_continue (NULL);
  
  return FALSE;
}


static gboolean
xfsm_manager_choose_session (XfceRc *rc)
{
  gchar *name;
  gboolean load;
  gchar **groups;
  gint n;
  gint sessions = 0;

  /* check if there are any sessions to load */
  groups = xfce_rc_get_groups (rc);
  for (n = 0; groups[n] != NULL; ++n)
    if (strncmp (groups[n], "Session: ", 9) == 0)
      ++sessions;
  g_strfreev (groups);

  if (sessions == 0)
    return FALSE;
  
  load = xfsm_splash_screen_choose (splash_screen, rc, session_name, &name);

  if (session_name != NULL)
    g_free (session_name);
  session_name = name;

  return load;
}


static gboolean
xfsm_manager_load_session (void)
{
  XfsmProperties *properties;
  gchar           buffer[1024];
  XfceRc         *rc;
  gint            count;
  
  if (!g_file_test (session_file, G_FILE_TEST_IS_REGULAR))
    return FALSE;

  rc = xfce_rc_simple_open (session_file, FALSE);
  if (G_UNLIKELY (rc == NULL))
    return FALSE;
  
  if (!xfsm_manager_choose_session (rc))
    {
      xfce_rc_close (rc);
      return FALSE;
    }

  g_snprintf (buffer, 1024, "Session: %s", session_name);
  
  xfce_rc_set_group (rc, buffer);
  count = xfce_rc_read_int_entry (rc, "Count", 0);
  if (G_UNLIKELY (count <= 0))
    {
      xfce_rc_close (rc);
      return FALSE;
    }
  
  while (count-- > 0)
    {
      g_snprintf (buffer, 1024, "Client%d_", count);
      properties = xfsm_properties_load (rc, buffer);
      if (G_UNLIKELY (properties == NULL))
        continue;
      if (xfsm_properties_check (properties))
        pending_properties = g_list_append (pending_properties, properties);
      else
        xfsm_properties_free (properties);
    }

  /* load legacy applications */
  xfsm_legacy_load_session (rc);

  xfce_rc_close (rc);

  return pending_properties != NULL;
}


static gboolean
xfsm_manager_load_failsafe (XfceRc *rc)
{
  FailsafeClient *fclient;
  const gchar    *old_group;
  GdkDisplay     *display;
  gchar         **command;
  gchar           command_entry[256];
  gchar           screen_entry[256];
  gint            count;
  gint            i;
  gint            n_screen;
  
  if (!xfce_rc_has_group (rc, "Failsafe Session"))
    return FALSE;

  display = gdk_display_get_default ();
  old_group = xfce_rc_get_group (rc);
  xfce_rc_set_group (rc, "Failsafe Session");
  
  count = xfce_rc_read_int_entry (rc, "Count", 0);

  for (i = 0; i < count; ++i)
    {
      g_snprintf (command_entry, 256, "Client%d_Command", i);
      command = xfce_rc_read_list_entry (rc, command_entry, NULL);
      if (G_UNLIKELY (command == NULL))
        continue;

      g_snprintf (screen_entry, 256, "Client%d_PerScreen", i);
      if (xfce_rc_read_bool_entry (rc, screen_entry, FALSE))
        {
          for (n_screen = 0; n_screen < gdk_display_get_n_screens (display); ++n_screen)
            {
              fclient = g_new0 (FailsafeClient, 1);
              fclient->command = xfce_rc_read_list_entry (rc, command_entry, NULL);
              fclient->screen = gdk_display_get_screen (display, n_screen);
              failsafe_clients = g_list_append (failsafe_clients, fclient);
            }
        }
      else
        {
          fclient = g_new0 (FailsafeClient, 1);
          fclient->command = command;
          fclient->screen = gdk_screen_get_default ();
          failsafe_clients = g_list_append (failsafe_clients, fclient);
        }
    }

  xfce_rc_set_group (rc, old_group);

  return failsafe_clients != NULL;
}


static void
xfsm_manager_load_settings (XfceRc *rc)
{
  gboolean     session_loaded = FALSE;
  const gchar *name;
  
  name = xfce_rc_read_entry (rc, "SessionName", NULL);
  if (name != NULL && *name != '\0')
    session_name = g_strdup (name);
  else
    session_name = g_strdup (DEFAULT_SESSION_NAME);

  session_loaded = xfsm_manager_load_session ();

  if (session_loaded)
    {
      xfsm_verbose ("Session \"%s\" loaded successfully.\n\n", session_name);
      failsafe_mode = FALSE;
    }
  else
    {
      if (!xfsm_manager_load_failsafe (rc))
        {
          fprintf (stderr, "xfce4-session: Unable to load failsafe session, exiting.\n");
          xfce_rc_close (rc);
          exit (EXIT_FAILURE);
        }
      failsafe_mode = TRUE;
    }
}


void
xfsm_manager_init (XfceRc *rc)
{
  gchar *display_name;
  gchar *resource_name;

  xfce_rc_set_group (rc, "General");
  display_name  = xfce_gdk_display_get_fullname (gdk_display_get_default ());
  resource_name = g_strconcat ("sessions/xfce4-session-", display_name, NULL);
  session_file  = xfce_resource_save_location (XFCE_RESOURCE_CACHE, resource_name, TRUE);
  g_free (resource_name);
  g_free (display_name);

  xfsm_manager_load_settings (rc);
}


gboolean
xfsm_manager_restart (void)
{
  /* setup legacy application handling */
  xfsm_legacy_init ();

  g_idle_add ((GSourceFunc) xfsm_manager_startup, NULL);
  
  return TRUE;
}


gchar*
xfsm_manager_generate_client_id (SmsConn sms_conn)
{
  static char *addr = NULL;
  static int   sequence = 0;
  char        *sms_id;
  char        *id = NULL;

  if (sms_conn != NULL)
    {
      sms_id = SmsGenerateClientID (sms_conn);
      if (sms_id != NULL)
        {
          id = g_strdup (sms_id);
          free (sms_id);
        }
    }

  if (id == NULL)
    {
      if (addr == NULL)
        {
          /*
           * Faking our IP address, the 0 below is "unknown"
           * address format (1 would be IP, 2 would be DEC-NET
           * format). Stolen from KDE :-)
           */
          addr = g_strdup_printf ("0%.8x", g_random_int ());
        }

      id = (char *) g_malloc (50);
      g_snprintf (id, 50, "1%s%.13ld%.10d%.4d", addr,
                  (long) time (NULL), (int) getpid (), sequence);
      sequence = (sequence + 1) % 10000;
    }

  return id;
}


XfsmClient*
xfsm_manager_new_client (SmsConn  sms_conn,
                         gchar  **error)
{
  if (G_UNLIKELY (state != XFSM_MANAGER_IDLE)
      && G_UNLIKELY (state != XFSM_MANAGER_STARTUP))
    {
      if (error != NULL)
        *error = "We don't accept clients while in CheckPoint/Shutdown state!";
      return NULL;
    }

  return xfsm_client_new (sms_conn);
}


gboolean
xfsm_manager_register_client (XfsmClient  *client,
                              const gchar *previous_id)
{
  XfsmProperties *properties = NULL;
  gchar          *client_id;
  GList          *lp;
  
  if (previous_id != NULL)
    {
      for (lp = starting_properties; lp != NULL; lp = lp->next)
        {
          if (strcmp (XFSM_PROPERTIES (lp->data)->client_id, previous_id) == 0)
            {
              properties = XFSM_PROPERTIES (lp->data);
              starting_properties = g_list_remove (starting_properties,
                                                   properties);
              break;
            }
        }

      if (properties == NULL)
        {
          for (lp = pending_properties; lp != NULL; lp = lp->next)
            {
              if (!strcmp (XFSM_PROPERTIES (lp->data)->client_id, previous_id))
                {
                  properties = XFSM_PROPERTIES (lp->data);
                  pending_properties = g_list_remove (pending_properties,
                                                      properties);
                  break;
                }
            }
        }

      /* If previous_id is invalid, the SM will send a BadValue error message
       * to the client and reverts to register state waiting for another
       * RegisterClient message.
       */
      if (properties == NULL)
        return FALSE;

      xfsm_client_set_initial_properties (client, properties);
    }
  else
    {
      client_id = xfsm_manager_generate_client_id (client->sms_conn);
      properties = xfsm_properties_new (client_id,
                                        SmsClientHostName (client->sms_conn));
      xfsm_client_set_initial_properties (client, properties);
      g_free (client_id);
    }

  running_clients = g_list_append (running_clients, client);

  SmsRegisterClientReply (client->sms_conn, (char *) client->id);
  
  if (previous_id == NULL)
    {
      SmsSaveYourself (client->sms_conn, SmSaveLocal, False, SmInteractStyleNone, False);
      client->state = XFSM_CLIENT_SAVINGLOCAL;
      client->save_timeout_id = g_timeout_add (SAVE_TIMEOUT,
                                               xfsm_manager_save_timeout,
                                               client);
    }

  if ((failsafe_mode || previous_id != NULL) && state == XFSM_MANAGER_STARTUP)
    {
      /* Only continue the startup if we are either in Failsafe mode (which
       * means that we don't have any previous_id at all) or the previous_id
       * matched one of the starting_properties. If there was no match
       * above, previous_id will be NULL here.
       * See http://bugs.xfce.org/view_bug_page.php?f_id=212 for details.
       */
      xfsm_manager_startup_continue (previous_id);
    }

  return TRUE;
}


void
xfsm_manager_start_interact (XfsmClient *client)
{
  /* notify client of interact */
  SmsInteract (client->sms_conn);
  client->state = XFSM_CLIENT_INTERACTING;
  
  /* stop save yourself timeout */
  g_source_remove (client->save_timeout_id);
  client->save_timeout_id = 0;
}


void
xfsm_manager_interact (XfsmClient *client,
                       int         dialog_type)
{
  GList *lp;
  
  if (G_UNLIKELY (client->state != XFSM_CLIENT_SAVING))
    {
      xfsm_verbose ("Client Id = %s, requested INTERACT, but client is not in SAVING mode\n"
                    "   Client will be disconnected now.\n\n",
                    client->id);
      xfsm_manager_close_connection (client, TRUE);
    }
  else if (G_UNLIKELY (state != XFSM_MANAGER_CHECKPOINT)
        && G_UNLIKELY (state != XFSM_MANAGER_SHUTDOWN))
    {
      xfsm_verbose ("Client Id = %s, requested INTERACT, but manager is not in CheckPoint/Shutdown mode\n"
                    "   Clinet will be disconnected now.\n\n",
                    client->id);
      xfsm_manager_close_connection (client, TRUE);
    }
  else
    {
      for (lp = running_clients; lp != NULL; lp = lp->next)
        if (XFSM_CLIENT (lp->data)->state == XFSM_CLIENT_INTERACTING)
          {
            client->state = XFSM_CLIENT_WAITFORINTERACT;
            return;
          }
  
      xfsm_manager_start_interact (client);
    }
}


void
xfsm_manager_interact_done (XfsmClient *client,
                            gboolean    cancel_shutdown)
{
  GList *lp;
  
  if (G_UNLIKELY (client->state != XFSM_CLIENT_INTERACTING))
    {
      xfsm_verbose ("Client Id = %s, send INTERACT DONE, but client is not in INTERACTING state\n"
                    "   Client will be disconnected now.\n\n",
                    client->id);
      xfsm_manager_close_connection (client, TRUE);
      return;
    }
  else if (G_UNLIKELY (state != XFSM_MANAGER_CHECKPOINT)
        && G_UNLIKELY (state != XFSM_MANAGER_SHUTDOWN))
    {
      xfsm_verbose ("Client Id = %s, send INTERACT DONE, but manager is not in CheckPoint/Shutdown state\n"
                    "   Client will be disconnected now.\n\n",
                    client->id);
      xfsm_manager_close_connection (client, TRUE);
      return;
    }
  
  client->state = XFSM_CLIENT_SAVING;
  
  /* Setting the cancel-shutdown field to True indicates that the user
   * has requested that the entire shutdown be cancelled. Cancel-
   * shutdown may only be True if the corresponding SaveYourself
   * message specified True for the shutdown field and Any or Error
   * for the interact-style field. Otherwise, cancel-shutdown must be
   * False.
   */
  if (cancel_shutdown && state == XFSM_MANAGER_SHUTDOWN)
    {
      /* we go into checkpoint state from here... */
      state = XFSM_MANAGER_CHECKPOINT;
      
      /* If a shutdown is in progress, the user may have the option
       * of cancelling the shutdown. If the shutdown is cancelled
       * (specified in the "Interact Done" message), the session
       * manager should send a "Shutdown Cancelled" message to each
       * client that requested to interact.
       */
      for (lp = running_clients; lp != NULL; lp = lp->next)
        {
          if (XFSM_CLIENT (lp->data)->state != XFSM_CLIENT_WAITFORINTERACT)
            continue;

          /* reset all clients that are waiting for interact */
          client->state = XFSM_CLIENT_SAVING;
          SmsShutdownCancelled (XFSM_CLIENT (lp->data)->sms_conn);
        }
    }
  else
    {
      /* let next client interact */
      for (lp = running_clients; lp != NULL; lp = lp->next)
        if (XFSM_CLIENT (lp->data)->state == XFSM_CLIENT_WAITFORINTERACT)
          {
            xfsm_manager_start_interact (XFSM_CLIENT (lp->data));
            break;
          }
    }

  /* restart save yourself timeout for client */
  client->save_timeout_id = g_timeout_add (SAVE_TIMEOUT,
                                           xfsm_manager_save_timeout,
                                           client);
}


void
xfsm_manager_save_yourself (XfsmClient *client,
                            gint        save_type,
                            gboolean    shutdown,
                            gint        interact_style,
                            gboolean    fast,
                            gboolean    global)
{
  gboolean shutdown_save = TRUE;
  GList   *lp;

  if (G_UNLIKELY (client->state != XFSM_CLIENT_IDLE))
    {
      xfsm_verbose ("Client Id = %s, requested SAVE YOURSELF, but client is not in IDLE mode.\n"
                    "   Client will be nuked now.\n\n",
                    client->id);
      xfsm_manager_close_connection (client, TRUE);
      return;
    }
  else if (G_UNLIKELY (state != XFSM_MANAGER_IDLE))
    {
      xfsm_verbose ("Client Id = %s, requested SAVE YOURSELF, but manager is not in IDLE mode.\n"
                    "   Client will be nuked now.\n\n",
                    client->id);
      xfsm_manager_close_connection (client, TRUE);
      return;
    }
  
  if (!global)
    {
      /* client requests a local checkpoint. We slightly ignore
       * shutdown here, since it does not make sense for a local
       * checkpoint.
       */
      SmsSaveYourself (client->sms_conn, save_type, FALSE, interact_style, fast);
      client->state = XFSM_CLIENT_SAVINGLOCAL;
      client->save_timeout_id = g_timeout_add (SAVE_TIMEOUT,
                                               xfsm_manager_save_timeout,
                                               client);
    }
  else
    {
      if (!fast && shutdown && !shutdownDialog (&shutdown_type, &shutdown_save))
        return;
  
      if (!shutdown || shutdown_save)
        {
          state = shutdown ? XFSM_MANAGER_SHUTDOWN : XFSM_MANAGER_CHECKPOINT;
          
          /* handle legacy applications first! */
          xfsm_legacy_perform_session_save ();

          for (lp = running_clients; lp != NULL; lp = lp->next)
            {
              XfsmClient *client = XFSM_CLIENT (lp->data);

              /* xterm's session management is broken, so we won't
               * send a SAVE YOURSELF to xterms */
              if (client->properties->program != NULL
                  && strcasecmp (client->properties->program, "xterm") == 0)
                {
                  continue;
                }

              if (client->state != XFSM_CLIENT_SAVINGLOCAL)
                {
                  SmsSaveYourself (client->sms_conn, save_type, shutdown,
                                   interact_style, fast);
                }

              client->state = XFSM_CLIENT_SAVING;
              client->save_timeout_id = g_timeout_add (SAVE_TIMEOUT,
                                                       xfsm_manager_save_timeout,
                                                       client);
            } 
        }
      else
        {
          /* shutdown session without saving */
          xfsm_manager_perform_shutdown ();
        }
    }
}


void
xfsm_manager_save_yourself_phase2 (XfsmClient *client)
{
  if (state != XFSM_MANAGER_CHECKPOINT && state != XFSM_MANAGER_SHUTDOWN)
    {
      SmsSaveYourselfPhase2 (client->sms_conn);
      client->state = XFSM_CLIENT_SAVINGLOCAL;
      g_source_remove (client->save_timeout_id);
      client->save_timeout_id = g_timeout_add (SAVE_TIMEOUT,
                                               xfsm_manager_save_timeout,
                                               client);
    }
  else
    {
      client->state = XFSM_CLIENT_WAITFORPHASE2;
      g_source_remove (client->save_timeout_id);
      client->save_timeout_id = 0;

      if (!xfsm_manager_check_clients_saving ())
        xfsm_manager_maybe_enter_phase2 ();
    }
}


void
xfsm_manager_save_yourself_done (XfsmClient *client,
                                 gboolean    success)
{
  if (client->state != XFSM_CLIENT_SAVING && client->state != XFSM_CLIENT_SAVINGLOCAL)
    {
      xfsm_verbose ("Client Id = %s send SAVE YOURSELF DONE, while not being "
                    "in save mode. Prepare to be nuked!\n",
                    client->id);
      
      xfsm_manager_close_connection (client, TRUE);
    }

  /* remove client save timeout, as client responded in time */  
  g_source_remove (client->save_timeout_id);
  client->save_timeout_id = 0;

  if (client->state == XFSM_CLIENT_SAVINGLOCAL)
    {
      /* client completed local SaveYourself */
      client->state = XFSM_CLIENT_IDLE;
      SmsSaveComplete (client->sms_conn);
    }
  else if (state != XFSM_MANAGER_CHECKPOINT && state != XFSM_MANAGER_SHUTDOWN)
    {
      xfsm_verbose ("Client Id = %s, send SAVE YOURSELF DONE, but manager is not in CheckPoint/Shutdown mode.\n"
                    "   Client will be nuked now.\n\n",
                    client->id);
      xfsm_manager_close_connection (client, TRUE);
    }
  else
    {
      client->state = XFSM_CLIENT_SAVEDONE;
      xfsm_manager_complete_saveyourself ();
    }
}


void
xfsm_manager_close_connection (XfsmClient *client,
                               gboolean    cleanup)
{
  IceConn ice_conn;
  GList  *lp;
  
  client->state = XFSM_CLIENT_DISCONNECTED;
  if (client->save_timeout_id > 0)
    {
      g_source_remove (client->save_timeout_id);
      client->save_timeout_id = 0;
    }

  if (cleanup)
    {
      ice_conn = SmsGetIceConnection (client->sms_conn);
      SmsCleanUp (client->sms_conn);
      IceSetShutdownNegotiation (ice_conn, False);
      IceCloseConnection (ice_conn);
    }
  
  if (state == XFSM_MANAGER_SHUTDOWNPHASE2)
    {
      for (lp = running_clients; lp != NULL; lp = lp->next)
        if (XFSM_CLIENT (lp->data)->state != XFSM_CLIENT_DISCONNECTED)
          return;
      
      /* all clients finished the DIE phase in time */
      g_source_remove (die_timeout_id);
      gtk_main_quit ();
    }
  else if (state == XFSM_MANAGER_SHUTDOWN || state == XFSM_MANAGER_CHECKPOINT)
    {
      xfsm_verbose ("Client Id = %s, closed connection in checkpoint state\n"
                    "   Session manager will show NO MERCY\n\n",
                    client->id);
      
      /* stupid client disconnected in CheckPoint state, prepare to be nuked! */
      running_clients = g_list_remove (running_clients, client);
      xfsm_client_free (client);
      xfsm_manager_complete_saveyourself ();
    }
  else
    {
      XfsmProperties *properties = client->properties;
      
      if (properties != NULL && xfsm_properties_check (properties))
        {
          if (properties->restart_style_hint == SmRestartAnyway)
            {
              restart_properties = g_list_append (restart_properties, properties);
              client->properties = NULL;
            }
          else if (properties->restart_style_hint == SmRestartImmediately)
            {
              if (++properties->restart_attempts > MAX_RESTART_ATTEMPTS)
                {
                  xfsm_verbose ("Client Id = %s, reached maximum attempts [Restart attempts = %d]\n"
                                "   Will be re-scheduled for run on next startup\n",
                                properties->client_id, properties->restart_attempts);

                  restart_properties = g_list_append (restart_properties, properties);
                  client->properties = NULL;
                }
#if 0
              else if (xfsm_manager_run_prop_command (properties, SmRestartCommand))
                {
                  /* XXX - add a timeout here, in case the application does not come up */
                  pending_properties = g_list_append (pending_properties, properties);
                  client->properties = NULL;
                }
#endif
            }
        }

      running_clients = g_list_remove (running_clients, client);
      xfsm_client_free (client);
    }
}


void
xfsm_manager_close_connection_by_ice_conn (IceConn ice_conn)
{
  GList *lp;
  
  for (lp = running_clients; lp != NULL; lp = lp->next)
    if (SmsGetIceConnection (XFSM_CLIENT (lp->data)->sms_conn) == ice_conn)
      {
        xfsm_manager_close_connection (XFSM_CLIENT (lp->data), FALSE);
        break;
      }

  /* be sure to close the Ice connection in any case */
  IceSetShutdownNegotiation (ice_conn, False);
  IceCloseConnection (ice_conn);
}


void
xfsm_manager_perform_shutdown (void)
{          
  GList *lp;
  
  /* send SmDie message to all clients */
  state = XFSM_MANAGER_SHUTDOWNPHASE2;
  for (lp = running_clients; lp != NULL; lp = lp->next)
    SmsDie (XFSM_CLIENT (lp->data)->sms_conn);

  /* give all clients the chance to close the connection */
  die_timeout_id = g_timeout_add (DIE_TIMEOUT,
                                  (GSourceFunc) gtk_main_quit,
                                  NULL);
}


gboolean
xfsm_manager_check_clients_saving (void)
{
  GList *lp;
  
  for (lp = running_clients; lp != NULL; lp = lp->next)
    if (XFSM_CLIENT (lp->data)->state == XFSM_CLIENT_SAVING)
      return TRUE;
  
  return FALSE;
}


gboolean
xfsm_manager_maybe_enter_phase2 (void)
{
  gboolean entered_phase2 = FALSE;
  GList   *lp;
  
  for (lp = running_clients; lp != NULL; lp = lp->next)
    { 
      XfsmClient *client = XFSM_CLIENT (lp->data);
      
      if (client->state == XFSM_CLIENT_WAITFORPHASE2)
        {
          entered_phase2 = TRUE;
          SmsSaveYourselfPhase2 (client->sms_conn);
          client->state = XFSM_CLIENT_SAVING;
          client->save_timeout_id = g_timeout_add (SAVE_TIMEOUT,
                                                   xfsm_manager_save_timeout,
                                                   client);

          xfsm_verbose ("Client Id = %s enters SAVE YOURSELF PHASE2.\n\n",
                        client->id);
        }
    }

  return entered_phase2;
}


void
xfsm_manager_complete_saveyourself (void)
{
  GList *lp;
  
  /* Check if still clients in SAVING state or if we have to enter PHASE2
   * now. In either case, SaveYourself cannot be completed in this run.
   */
  if (xfsm_manager_check_clients_saving () || xfsm_manager_maybe_enter_phase2 ())
    return;
  
  xfsm_verbose ("Manager finished SAVE YOURSELF, session data will be stored now.\n\n");
  
  /* all clients done, store session data */
  xfsm_manager_store_session ();
  
  if (state == XFSM_MANAGER_CHECKPOINT)
    {
      /* reset all clients to idle state */
      state = XFSM_MANAGER_IDLE;
      for (lp = running_clients; lp != NULL; lp = lp->next)
        {
          XFSM_CLIENT (lp->data)->state = XFSM_CLIENT_IDLE;
          SmsSaveComplete (XFSM_CLIENT (lp->data)->sms_conn);
        }
    }
  else
    {
      /* shutdown the session */
      xfsm_manager_perform_shutdown ();
    }
}


gboolean
xfsm_manager_save_timeout (gpointer client_data)
{
  XfsmClient *client = XFSM_CLIENT (client_data);
  
  xfsm_verbose ("Client id = %s, received SAVE TIMEOUT\n"
                "   Client will be disconnected now.\n\n",
                client->id);

  xfsm_manager_close_connection (client, TRUE);
  
  return FALSE;
}


void
xfsm_manager_store_session (void)
{
  NetkWorkspace *workspace;
  GdkDisplay    *display;
  NetkScreen    *screen;
  XfceRc        *rc;
  GList         *lp;
  gchar          prefix[64];
  gchar         *backup;
  gchar         *group;
  gint           count = 0;
  gint           n, m;

  rc = xfce_rc_simple_open (session_file, FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      fprintf (stderr,
               "xfce4-session: Unable to open session file %s for "
               "writing. Session data will not be stored. Please check "
               "your installation.\n",
               session_file);
      return;
    }

  /* backup the old session file first */
  if (g_file_test (session_file, G_FILE_TEST_IS_REGULAR))
    {
      backup = g_strconcat (session_file, ".bak", NULL);
      unlink (backup);
      link (session_file, backup);
      g_free (backup);
    }

  group = g_strconcat ("Session: ", session_name, NULL);
  xfce_rc_delete_group (rc, group, TRUE);
  xfce_rc_set_group (rc, group);
  g_free (group);

  for (lp = restart_properties; lp != NULL; lp = lp->next)
    {
      g_snprintf (prefix, 64, "Client%d_", count);
      xfsm_properties_store (XFSM_PROPERTIES (lp->data), rc, prefix);
      ++count;
    }

  for (lp = running_clients; lp != NULL; lp = lp->next)
    {
      if (XFSM_CLIENT (lp->data)->properties == NULL
          || !xfsm_properties_check (XFSM_CLIENT (lp->data)->properties))
        continue;
      
      g_snprintf (prefix, 64, "Client%d_", count);
      xfsm_properties_store (XFSM_CLIENT (lp->data)->properties, rc, prefix);
      ++count;
    }

  xfce_rc_write_int_entry (rc, "Count", count);

  /* store legacy applications state */
  xfsm_legacy_store_session (rc);

  /* store current workspace numbers */
  display = gdk_display_get_default ();
  for (n = 0; n < gdk_display_get_n_screens (display); ++n)
    {
      screen = netk_screen_get (n);
      netk_screen_force_update (screen);
      
      workspace = netk_screen_get_active_workspace (screen);
      m = netk_workspace_get_number (workspace);

      g_snprintf (prefix, 64, "Screen%d_ActiveWorkspace", n);
      xfce_rc_write_int_entry (rc, prefix, m);
    }

  /* remember time */
  xfce_rc_write_int_entry (rc, "LastAccess", time (NULL));

  xfce_rc_close (rc);
}


