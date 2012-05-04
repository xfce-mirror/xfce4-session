/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2011      Nick Schermer <nick@xfce.org>
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
 *
 * Parts of this file where taken from gnome-session/logout.c, which
 * was written by Owen Taylor <otaylor@redhat.com>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif


#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <libxfce4util/libxfce4util.h>
#include <gtk/gtk.h>

#include <libxfsm/xfsm-util.h>

#include <xfce4-session/xfsm-shutdown.h>
#include <xfce4-session/xfsm-compat-gnome.h>
#include <xfce4-session/xfsm-compat-kde.h>
#include <xfce4-session/xfsm-fadeout.h>
#include <xfce4-session/xfsm-global.h>
#include <xfce4-session/xfsm-legacy.h>
#include <xfce4-session/xfsm-consolekit.h>
#include <xfce4-session/xfsm-upower.h>



static void xfsm_shutdown_finalize  (GObject      *object);
static void xfsm_shutdown_sudo_free (XfsmShutdown *shutdown);



struct _XfsmShutdownClass
{
  GObjectClass __parent__;
};

typedef enum
{
  SUDO_NOT_INITIAZED,
  SUDO_AVAILABLE,
  SUDO_FAILED
}
HelperState;

struct _XfsmShutdown
{
  GObject __parent__;

  XfsmConsolekit *consolekit;
  XfsmUPower     *upower;

  /* kiosk settings */
  gboolean        kiosk_can_shutdown;
  gboolean        kiosk_can_save_session;

  /* sudo helper */
  HelperState     helper_state;
  pid_t           helper_pid;
  FILE           *helper_infile;
  FILE           *helper_outfile;
  guint           helper_watchid;
  gboolean        helper_require_password;
};



G_DEFINE_TYPE (XfsmShutdown, xfsm_shutdown, G_TYPE_OBJECT)



static void
xfsm_shutdown_class_init (XfsmShutdownClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfsm_shutdown_finalize;
}



static void
xfsm_shutdown_init (XfsmShutdown *shutdown)
{
  XfceKiosk *kiosk;

  shutdown->consolekit = xfsm_consolekit_get ();
  shutdown->upower = xfsm_upower_get ();
  shutdown->helper_state = SUDO_NOT_INITIAZED;
  shutdown->helper_require_password = FALSE;

  /* check kiosk */
  kiosk = xfce_kiosk_new ("xfce4-session");
  shutdown->kiosk_can_shutdown = xfce_kiosk_query (kiosk, "Shutdown");
  shutdown->kiosk_can_save_session = xfce_kiosk_query (kiosk, "SaveSession");
  xfce_kiosk_free (kiosk);
}



static void
xfsm_shutdown_finalize (GObject *object)
{
  XfsmShutdown *shutdown = XFSM_SHUTDOWN (object);

  g_object_unref (G_OBJECT (shutdown->consolekit));
  g_object_unref (G_OBJECT (shutdown->upower));

  /* close down helper */
  xfsm_shutdown_sudo_free (shutdown);

  (*G_OBJECT_CLASS (xfsm_shutdown_parent_class)->finalize) (object);
}



static void
xfsm_shutdown_sudo_free (XfsmShutdown *shutdown)
{
  gint status;

  /* close down helper */
  if (shutdown->helper_infile != NULL)
    {
      fclose (shutdown->helper_infile);
      shutdown->helper_infile = NULL;
    }

  if (shutdown->helper_outfile != NULL)
    {
      fclose (shutdown->helper_outfile);
      shutdown->helper_outfile = NULL;
    }

  if (shutdown->helper_watchid > 0)
    {
      g_source_remove (shutdown->helper_watchid);
      shutdown->helper_watchid = 0;
    }

  if (shutdown->helper_pid > 0)
    {
      waitpid (shutdown->helper_pid, &status, 0);
      shutdown->helper_pid = 0;
    }

  /* reset state */
  shutdown->helper_state = SUDO_NOT_INITIAZED;
}



static void
xfsm_shutdown_sudo_childwatch (GPid     pid,
                               gint     status,
                               gpointer data)
{
  /* close down sudo stuff */
  xfsm_shutdown_sudo_free (XFSM_SHUTDOWN (data));
}



static gboolean
xfsm_shutdown_sudo_init (XfsmShutdown  *shutdown,
                         GError       **error)
{
  gchar  *cmd;
  struct  rlimit rlp;
  gchar   buf[15];
  gint    parent_pipe[2];
  gint    child_pipe[2];
  gint    n;

  /* return state if we succeeded */
  if (shutdown->helper_state != SUDO_NOT_INITIAZED)
    return shutdown->helper_state == SUDO_AVAILABLE;

  g_return_val_if_fail (shutdown->helper_infile == NULL, FALSE);
  g_return_val_if_fail (shutdown->helper_outfile == NULL, FALSE);
  g_return_val_if_fail (shutdown->helper_watchid == 0, FALSE);
  g_return_val_if_fail (shutdown->helper_pid == 0, FALSE);

  /* assume it won't work for now */
  shutdown->helper_state = SUDO_FAILED;

  cmd = g_find_program_in_path ("sudo");
  if (G_UNLIKELY (cmd == NULL))
    {
      g_set_error_literal (error, 1, 0,
                           "The program \"sudo\" was not found");
      return FALSE;
    }

  if (pipe (parent_pipe) == -1)
    {
      g_set_error (error, 1, 0,
                   "Unable to create parent pipe: %s",
                   strerror (errno));
      goto err0;
    }

  if (pipe (child_pipe) == -1)
    {
      g_set_error (error, 1, 0,
                   "Unable to create child pipe: %s",
                   strerror (errno));
      goto err1;
    }

  shutdown->helper_pid = fork ();
  if (shutdown->helper_pid < 0)
    {
      g_set_error (error, 1, 0,
                   "Unable to fork sudo helper: %s",
                   strerror (errno));
      goto err2;
    }
  else if (shutdown->helper_pid == 0)
    {
      /* setup signals */
      signal (SIGPIPE, SIG_IGN);

      /* setup environment */
      g_setenv ("LC_ALL", "C", TRUE);
      g_setenv ("LANG", "C", TRUE);
      g_setenv ("LANGUAGE", "C", TRUE);

      /* setup the 3 standard file handles */
      dup2 (child_pipe[0], STDIN_FILENO);
      dup2 (parent_pipe[1], STDOUT_FILENO);
      dup2 (parent_pipe[1], STDERR_FILENO);

      /* close all other file handles */
      getrlimit (RLIMIT_NOFILE, &rlp);
      for (n = 0; n < (gint) rlp.rlim_cur; ++n)
        {
          if (n != STDIN_FILENO && n != STDOUT_FILENO && n != STDERR_FILENO)
            close (n);
        }

      /* execute sudo with the helper */
      execl (cmd, "sudo", "-H", "-S", "-p",
             "XFSM_SUDO_PASS ", "--",
             XFSM_SHUTDOWN_HELPER_CMD, NULL);

      g_free (cmd);
      cmd = NULL;

      _exit (127);
    }
  else
    {
      /* watch the sudo helper */
      shutdown->helper_watchid = g_child_watch_add (shutdown->helper_pid,
                                                    xfsm_shutdown_sudo_childwatch,
                                                    shutdown);
    }

  /* read sudo/helper answer */
  n = read (parent_pipe[0], buf, sizeof (buf));
  if (n < 15)
    {
      g_set_error (error, 1, 0,
                   "Unable to read response from sudo helper: %s",
                   n < 0 ? strerror (errno) : "Unknown error");
      goto err2;
    }

  /* open pipe to receive replies from sudo */
  shutdown->helper_infile = fdopen (parent_pipe[0], "r");
  if (shutdown->helper_infile == NULL)
    {
      g_set_error (error, 1, 0,
                   "Unable to open parent pipe: %s",
                   strerror (errno));
      goto err2;
    }
  close (parent_pipe[1]);

  /* open pipe to send passwords to sudo */
  shutdown->helper_outfile = fdopen (child_pipe[1], "w");
  if (shutdown->helper_outfile == NULL)
    {
      g_set_error (error, 1, 0,
                   "Unable to open parent pipe: %s",
                   strerror (errno));
      goto err2;
    }
  close (child_pipe[0]);

  /* check if NOPASSWD is set in /etc/sudoers */
  if (memcmp (buf, "XFSM_SUDO_PASS ", 15) == 0)
    {
      shutdown->helper_require_password = TRUE;
    }
  else if (memcmp (buf, "XFSM_SUDO_DONE ", 15) == 0)
    {
      shutdown->helper_require_password = FALSE;
    }
  else
    {
      g_set_error (error, 1, 0,
                   "Got unexpected reply from sudo shutdown helper");
      goto err2;
    }

  /* if we try again */
  shutdown->helper_state = SUDO_AVAILABLE;

  return TRUE;

err2:
  xfsm_shutdown_sudo_free (shutdown);

  close (child_pipe[0]);
  close (child_pipe[1]);

err1:
  close (parent_pipe[0]);
  close (parent_pipe[1]);

err0:
  g_free (cmd);

  shutdown->helper_pid = 0;

  return FALSE;
}



static gboolean
xfsm_shutdown_sudo_try_action (XfsmShutdown      *shutdown,
                               XfsmShutdownType   type,
                               GError           **error)
{
  const gchar *action;
  gchar        reply[256];

  g_return_val_if_fail (shutdown->helper_state == SUDO_AVAILABLE, FALSE);
  g_return_val_if_fail (shutdown->helper_outfile != NULL, FALSE);
  g_return_val_if_fail (shutdown->helper_infile != NULL, FALSE);
  g_return_val_if_fail (type == XFSM_SHUTDOWN_SHUTDOWN
                        || type == XFSM_SHUTDOWN_RESTART, FALSE);

  /* the command we send to sudo */
  if (type == XFSM_SHUTDOWN_SHUTDOWN)
    action = "POWEROFF";
  else if (type == XFSM_SHUTDOWN_RESTART)
    action = "REBOOT";
  else
    return FALSE;

  /* write action to sudo helper */
  if (fprintf (shutdown->helper_outfile, "%s\n", action) > 0)
    fflush (shutdown->helper_outfile);

  /* check if the write succeeded */
  if (ferror (shutdown->helper_outfile) != 0)
    {
      /* probably succeeded but the helper got killed */
      if (errno == EINTR)
        return TRUE;

      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   _("Error sending command to shutdown helper: %s"),
                   strerror (errno));
      return FALSE;
    }

  /* get responce from sudo helper */
  if (fgets (reply, sizeof (reply), shutdown->helper_infile) == NULL)
    {
      /* probably succeeded but the helper got killed */
      if (errno == EINTR)
        return TRUE;

      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   _("Error receiving response from shutdown helper: %s"),
                   strerror (errno));
      return FALSE;
    }

  if (strncmp (reply, "SUCCEED", 7) != 0)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("Shutdown command failed"));

      return FALSE;
    }

  return TRUE;
}



static XfsmPassState
xfsm_shutdown_sudo_send_password (XfsmShutdown  *shutdown,
                                  const gchar   *password)
{
  gchar        buf[1024];
  gsize        len_buf, len_send;
  gint         fd;
  gsize        len;
  gssize       result;
  gint         attempts;
  const gchar *errmsg = NULL;

  g_return_val_if_fail (shutdown->helper_state == SUDO_AVAILABLE, PASSWORD_FAILED);
  g_return_val_if_fail (shutdown->helper_outfile != NULL, PASSWORD_FAILED);
  g_return_val_if_fail (shutdown->helper_infile != NULL, PASSWORD_FAILED);
  g_return_val_if_fail (shutdown->helper_require_password, PASSWORD_FAILED);
  g_return_val_if_fail (password != NULL, PASSWORD_FAILED);

  /* write password to sudo helper */
  g_snprintf (buf, sizeof (buf), "%s\n", password);
  len_buf = strlen (buf);
  len_send = fwrite (buf, 1, len_buf, shutdown->helper_outfile);
  fflush (shutdown->helper_outfile);
  bzero (buf, len_buf);

  if (len_send != len_buf
      || ferror (shutdown->helper_outfile) != 0)
    {
      errmsg = "Failed to send password to sudo";
      goto err1;
    }

  fd = fileno (shutdown->helper_infile);

  for (len = 0, attempts = 0;;)
    {
      result = read (fd, buf + len, 256 - len);

      if (result < 0)
        {
          errmsg = "Failed to read data from sudo";
          goto err1;
        }
      else if (result == 0)
        {
          /* don't try too often */
          if (++attempts > 20)
            {
              errmsg = "Too many password attempts";
              goto err1;
            }

          continue;
        }
      else if (result + len >= sizeof (buf))
        {
          errmsg = "Received too much data from sudo";
          goto err1;
        }

      len += result;
      buf[len] = '\0';

      if (len >= 15)
        {
          if (g_str_has_suffix (buf, "XFSM_SUDO_PASS "))
            {
              return PASSWORD_RETRY;
            }
          else if (g_str_has_suffix (buf, "XFSM_SUDO_DONE "))
            {
              /* sudo is unlocked, no further passwords required */
              shutdown->helper_require_password = FALSE;

              return PASSWORD_SUCCEED;
            }
        }
    }

  return PASSWORD_FAILED;

  err1:

  g_printerr (PACKAGE_NAME ": %s.\n\n", errmsg);
  return PASSWORD_FAILED;
}



static gboolean
xfsm_shutdown_kiosk_can_shutdown (XfsmShutdown  *shutdown,
                                  GError       **error)
{
  if (!shutdown->kiosk_can_shutdown)
    {
      g_set_error_literal (error, 1, 0, _("Shutdown is blocked by the kiosk settings"));
      return FALSE;
    }

  return TRUE;
}



XfsmShutdown *
xfsm_shutdown_get (void)
{
  static XfsmShutdown *object = NULL;

  if (G_LIKELY (object != NULL))
    {
      g_object_ref (G_OBJECT (object));
    }
  else
    {
      object = g_object_new (XFSM_TYPE_SHUTDOWN, NULL);
      g_object_add_weak_pointer (G_OBJECT (object), (gpointer) &object);
    }

  return object;
}



gboolean
xfsm_shutdown_password_require (XfsmShutdown     *shutdown,
                                XfsmShutdownType  type)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  /* the helper only handled shutdown and restart */
  if (type == XFSM_SHUTDOWN_SHUTDOWN || type == XFSM_SHUTDOWN_RESTART)
    return shutdown->helper_require_password;

  return FALSE;
}



XfsmPassState
xfsm_shutdown_password_send (XfsmShutdown      *shutdown,
                             XfsmShutdownType   type,
                             const gchar       *password)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), PASSWORD_FAILED);
  g_return_val_if_fail (password != NULL, PASSWORD_FAILED);

  if ((type == XFSM_SHUTDOWN_SHUTDOWN || type == XFSM_SHUTDOWN_RESTART)
      && shutdown->helper_state == SUDO_AVAILABLE)
    return xfsm_shutdown_sudo_send_password (shutdown, password);

  return PASSWORD_FAILED;
}



gboolean
xfsm_shutdown_try_type (XfsmShutdown      *shutdown,
                        XfsmShutdownType   type,
                        GError           **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  switch (type)
    {
    case XFSM_SHUTDOWN_SHUTDOWN:
      return xfsm_shutdown_try_shutdown (shutdown, error);

    case XFSM_SHUTDOWN_RESTART:
      return xfsm_shutdown_try_restart (shutdown, error);

    case XFSM_SHUTDOWN_SUSPEND:
      return xfsm_shutdown_try_suspend (shutdown, error);

    case XFSM_SHUTDOWN_HIBERNATE:
      return xfsm_shutdown_try_hibernate (shutdown, error);

    default:
      g_set_error (error, 1, 0, _("Unknown shutdown method %d"), type);
      break;
    }

  return FALSE;
}



gboolean
xfsm_shutdown_try_restart (XfsmShutdown  *shutdown,
                           GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, error))
    return FALSE;

  if (shutdown->helper_state == SUDO_AVAILABLE)
    return xfsm_shutdown_sudo_try_action (shutdown, XFSM_SHUTDOWN_RESTART, error);
  else
    return xfsm_consolekit_try_restart (shutdown->consolekit, error);
}



gboolean
xfsm_shutdown_try_shutdown (XfsmShutdown  *shutdown,
                            GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, error))
    return FALSE;

  if (shutdown->helper_state == SUDO_AVAILABLE)
    return xfsm_shutdown_sudo_try_action (shutdown, XFSM_SHUTDOWN_SHUTDOWN, error);
  else
    return xfsm_consolekit_try_shutdown (shutdown->consolekit, error);
}



gboolean
xfsm_shutdown_try_suspend (XfsmShutdown  *shutdown,
                           GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  return xfsm_upower_try_suspend (shutdown->upower, error);
}



gboolean
xfsm_shutdown_try_hibernate (XfsmShutdown  *shutdown,
                             GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  return xfsm_upower_try_hibernate (shutdown->upower, error);
}



gboolean
xfsm_shutdown_can_restart (XfsmShutdown  *shutdown,
                           gboolean      *can_restart,
                           GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_restart = FALSE;
      return TRUE;
    }

  if (xfsm_consolekit_can_restart (shutdown->consolekit, can_restart, error))
    return TRUE;

  if (xfsm_shutdown_sudo_init (shutdown, error))
    {
      *can_restart = TRUE;
      return TRUE;
    }

  return FALSE;
}



gboolean
xfsm_shutdown_can_shutdown (XfsmShutdown  *shutdown,
                            gboolean      *can_shutdown,
                            GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_shutdown = FALSE;
      return TRUE;
    }

  if (xfsm_consolekit_can_shutdown (shutdown->consolekit, can_shutdown, error))
    return TRUE;

  if (xfsm_shutdown_sudo_init (shutdown, error))
    {
      *can_shutdown = TRUE;
      return TRUE;
    }

  return FALSE;
}



gboolean
xfsm_shutdown_can_suspend (XfsmShutdown  *shutdown,
                           gboolean      *can_suspend,
                           gboolean      *auth_suspend,
                           GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_suspend = FALSE;
      return TRUE;
    }

  return xfsm_upower_can_suspend (shutdown->upower, can_suspend, 
                                  auth_suspend, error);
}



gboolean
xfsm_shutdown_can_hibernate (XfsmShutdown  *shutdown,
                             gboolean      *can_hibernate,
                             gboolean      *auth_hibernate,
                             GError       **error)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);

  if (!xfsm_shutdown_kiosk_can_shutdown (shutdown, NULL))
    {
      *can_hibernate = FALSE;
      return TRUE;
    }

  return xfsm_upower_can_hibernate (shutdown->upower, can_hibernate,
                                    auth_hibernate, error);
}



gboolean
xfsm_shutdown_can_save_session (XfsmShutdown *shutdown)
{
  g_return_val_if_fail (XFSM_IS_SHUTDOWN (shutdown), FALSE);
  return shutdown->kiosk_can_save_session;
}
