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

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <X11/ICE/ICElib.h>
#include <X11/ICE/ICEutil.h>
#include <X11/SM/SMlib.h>

#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/ice-layer.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-manager.h>

/* prototypes */
static void	 ice_error_handler     (IceConn);
static gboolean	 ice_process_messages  (GIOChannel  *channel,
                                        GIOCondition condition,
                                        gpointer     user_data);
static gboolean	 ice_connection_accept (GIOChannel  *channel,
                                        GIOCondition condition,
                                        gpointer     watch_data);
static FILE 	*ice_tmpfile           (char **name);
static void	 ice_auth_add          (FILE *,
                                        FILE *,
                                        char *,
                                        IceListenObj);

static char *auth_cleanup_file;


Bool
ice_auth_proc (char *hostname)
{
  return False;
}


static void
ice_error_handler (IceConn ice_conn)
{
  /*
   * The I/O error handlers does whatever is necessary to respond
   * to the I/O error and then returns, but it does not call
   * IceCloseConnection. The ICE connection is given a "bad IO"
   * status, and all future reads and writes to the connection
   * are ignored. The next time IceProcessMessages is called it
   * will return a status of IceProcessMessagesIOError. At that
   * time, the application should call IceCloseConnection.
   */
  xfsm_verbose ("ICE connection fd = %d, ICE I/O error on connection\n",
                IceConnectionNumber (ice_conn));
}


static gboolean
ice_process_messages (GIOChannel  *channel,
                      GIOCondition condition,
                      gpointer     user_data)
{
  IceProcessMessagesStatus status;
  IceConn                  ice_conn = (IceConn) user_data;

  status = IceProcessMessages (ice_conn, NULL, NULL);

  if (status == IceProcessMessagesIOError)
    {
      xfsm_manager_close_connection_by_ice_conn (ice_conn);

      /* remove the I/O watch */
      return FALSE;
    }

  /* keep the I/O watch running */
  return TRUE;
}


static void
ice_connection_watch (IceConn     ice_conn,
                      IcePointer  client_data,
                      Bool        opening,
                      IcePointer *watch_data)
{
  GIOChannel *channel;
  guint       watchid;
  gint        fd;

  if (opening)
    {
      fd = IceConnectionNumber (ice_conn);

      /* Make sure we don't pass on these file descriptors to an
       * exec'd child process.
       */
      fcntl (fd, F_SETFD, fcntl (fd, F_GETFD, 0) | FD_CLOEXEC);

      channel = g_io_channel_unix_new (fd);
      watchid = g_io_add_watch (channel, G_IO_ERR | G_IO_HUP | G_IO_IN,
                                ice_process_messages, ice_conn);
      g_io_channel_unref (channel);

      *watch_data = (IcePointer) GUINT_TO_POINTER (watchid);
    }
  else
    {
      watchid = GPOINTER_TO_UINT (*watch_data);
      g_source_remove (watchid);
    }
}


static gboolean
ice_connection_accept (GIOChannel  *channel,
                       GIOCondition condition,
                       gpointer     watch_data)
{
  IceConnectStatus cstatus;
  IceAcceptStatus  astatus;
  IceListenObj     ice_listener = (IceListenObj) watch_data;
  IceConn          ice_conn;

  ice_conn = IceAcceptConnection (ice_listener, &astatus);

  if (astatus != IceAcceptSuccess)
    {
      g_warning ("Failed to accept ICE connection on listener %p",
                 (gpointer) ice_listener);
    }
  else
    {
      /* Wait for the connection to leave pending state */
      do
        {
          IceProcessMessages (ice_conn, NULL, NULL);
        }
      while ((cstatus = IceConnectionStatus (ice_conn)) == IceConnectPending);

      if (cstatus != IceConnectAccepted)
        {
          if (cstatus == IceConnectIOError)
            {
              g_warning ("I/O error opening ICE connection %p", (gpointer) ice_conn);
            }
          else
            {
              g_warning ("ICE connection %p rejected", (gpointer) ice_conn);
            }

          IceSetShutdownNegotiation (ice_conn, False);
          IceCloseConnection (ice_conn);
        }
    }
  
  return TRUE;
}


static FILE*
ice_tmpfile (char **name)
{
  GError *error;
  mode_t  mode;
  FILE   *fp = NULL;
  int     fd;

  mode = umask (0077);

  fd = g_file_open_tmp(".xfsm-ICE-XXXXXX", name, &error);
  if (fd < 0)
    {
      g_warning ("Unable to open temporary file: %s", error->message);
      g_error_free (error);
    }
  else
    {
      fp = fdopen (fd, "wb");
    }

  umask (mode);

  return fp;
}


static void
fprintfhex (FILE *fp, int len, char *cp)
{
  static char hexchars[] = "0123456789abcdef";

  for (; len > 0; len--, cp++)
    {
      unsigned char s = *cp;
      putc (hexchars[s >> 4], fp);
      putc (hexchars[s & 0x0f], fp);
    }
}


static void
ice_auth_add (FILE        *setup_fp,
              FILE        *cleanup_fp,
              char        *protocol,
              IceListenObj ice_listener)
{
  IceAuthDataEntry entry;

  entry.protocol_name = protocol;
  entry.network_id = IceGetListenConnectionString (ice_listener);
  entry.auth_name = "MIT-MAGIC-COOKIE-1";
  entry.auth_data = IceGenerateMagicCookie (16);
  entry.auth_data_length = 16;

  IceSetPaAuthData (1, &entry);

  fprintf (setup_fp,
           "add %s \"\" %s MIT-MAGIC-COOKIE-1 ",
           protocol,
           entry.network_id);
  fprintfhex (setup_fp, 16, entry.auth_data);
  fprintf (setup_fp, "\n");

  fprintf (cleanup_fp,
           "remove protoname=%s protodata=\"\" netid=%s authname=MIT-MAGIC-COOKIE-1\n",
           protocol,
           entry.network_id);

  free (entry.network_id);
  free (entry.auth_data);
}


gboolean
ice_setup_listeners (int           num_listeners,
                     IceListenObj *listen_objs)
{
  GIOChannel *channel;
  char       *auth_setup_file;
  gchar      *command;
  FILE       *cleanup_fp;
  FILE       *setup_fp;
  int         fd;
  int         n;

  IceSetIOErrorHandler (ice_error_handler);
  IceAddConnectionWatch (ice_connection_watch, NULL);

  cleanup_fp = ice_tmpfile(&auth_cleanup_file);
  if (cleanup_fp == NULL)
    return FALSE;

  setup_fp = ice_tmpfile(&auth_setup_file);
  if (setup_fp == NULL)
    {
      fclose (cleanup_fp);
      unlink (auth_cleanup_file);
      g_free (auth_cleanup_file);
      return FALSE;
    }

  for (n = 0; n < num_listeners; n++)
    {
      fd = IceGetListenConnectionNumber (listen_objs[n]);

      /* Make sure we don't pass on these file descriptors to an
       * exec'd child process.
       */
      fcntl (fd, F_SETFD, fcntl (fd, F_GETFD, 0) | FD_CLOEXEC);

      channel = g_io_channel_unix_new (fd);
      g_io_add_watch (channel, G_IO_ERR | G_IO_HUP | G_IO_IN,
                      ice_connection_accept,
                      listen_objs[n]);
      g_io_channel_unref (channel);

      /* setup auth for this listener */
      ice_auth_add (setup_fp, cleanup_fp, "ICE", listen_objs[n]);
      ice_auth_add (setup_fp, cleanup_fp, "XSMP", listen_objs[n]);
      IceSetHostBasedAuthProc (listen_objs[n], ice_auth_proc);
    }

  fclose (setup_fp);
  fclose (cleanup_fp);

  /* setup ICE authority and remove setup file */
  command = g_strdup_printf ("iceauth source %s", auth_setup_file);
  system (command);
  g_free (command);
  unlink (auth_setup_file);
  g_free (auth_setup_file);

  return TRUE;
}

void
ice_cleanup (void)
{
  gchar *command;

  g_return_if_fail (auth_cleanup_file != NULL);

  /* remove newly added ICE authority entries */
  command = g_strdup_printf ("iceauth source %s", auth_cleanup_file);
  system (command);
  g_free (command);

  /* remove the cleanup file, no longer needed */
  unlink (auth_cleanup_file);
  g_free (auth_cleanup_file);
}


