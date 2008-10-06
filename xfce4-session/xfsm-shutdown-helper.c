/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_DBUS
#include <dbus/dbus.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include <xfce4-session/xfsm-shutdown-helper.h>



struct _XfsmShutdownHelper
{
  gchar   *sudo;
  pid_t    pid;
  FILE    *infile;
  FILE    *outfile;
  gboolean use_hal;
  gboolean need_password;
};



static gboolean
xfsm_shutdown_helper_hal_check (XfsmShutdownHelper *helper)
{
#ifdef HAVE_DBUS
  DBusConnection *connection;
  DBusMessage    *message;
  DBusMessage    *result;
  DBusError       error;

  /* initialize the error */
  dbus_error_init (&error);

  /* connect to the system message bus */
  connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
  if (G_UNLIKELY (connection == NULL))
    {
      g_warning (G_STRLOC ": Failed to connect to the system message bus: %s", error.message);
      dbus_error_free (&error);
      return FALSE;
    }

  /* this is a simple trick to check whether we are allowed to
   * use the org.freedesktop.Hal.Device.SystemPowerManagement
   * interface without shutting down/rebooting now.
   */
  message = dbus_message_new_method_call ("org.freedesktop.Hal",
                                          "/org/freedesktop/Hal/devices/computer",
                                          "org.freedesktop.Hal.Device.SystemPowerManagement",
                                          "ThisMethodMustNotExistInHal");
  result = dbus_connection_send_with_reply_and_block (connection, message, 2000, &error);
  dbus_message_unref (message);

  /* translate error results appropriately */
  if (result != NULL && dbus_set_error_from_message (&error, result))
    {
      /* release and reset the result */
      dbus_message_unref (result);
      result = NULL;
    }
  else if (G_UNLIKELY (result != NULL))
    {
      /* we received a valid message return?! HAL must be on crack! */
      dbus_message_unref (result);
      return FALSE;
    }

  /* if we receive org.freedesktop.DBus.Error.UnknownMethod, then
   * we are allowed to shutdown/reboot the computer via HAL.
   */
  if (strcmp (error.name, "org.freedesktop.DBus.Error.UnknownMethod") == 0)
    {
      dbus_error_free (&error);
      return TRUE;
    }

  /* otherwise, we failed for some reason */
  g_warning (G_STRLOC ": Failed to contact HAL: %s", error.message);
  dbus_error_free (&error);
#endif

  return FALSE;
}



static gboolean
xfsm_shutdown_helper_hal_send (XfsmShutdownHelper *helper,
                               XfsmShutdownCommand command)
{
#ifdef HAVE_DBUS
  DBusConnection *connection;
  DBusMessage    *message;
  DBusMessage    *result;
  DBusError       error;

  /* initialize the error */
  dbus_error_init (&error);

  /* connect to the system message bus */
  connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
  if (G_UNLIKELY (connection == NULL))
    {
      g_warning (G_STRLOC ": Failed to connect to the system message bus: %s", error.message);
      dbus_error_free (&error);
      return FALSE;
    }

  /* send the appropriate message to HAL, telling it to shutdown or reboot the system */
  message = dbus_message_new_method_call ("org.freedesktop.Hal",
                                          "/org/freedesktop/Hal/devices/computer",
                                          "org.freedesktop.Hal.Device.SystemPowerManagement",
                                          (command == XFSM_SHUTDOWN_COMMAND_REBOOT) ? "Reboot" : "Shutdown");
  result = dbus_connection_send_with_reply_and_block (connection, message, 2000, &error);
  dbus_message_unref (message);

  /* check if we received a result */
  if (G_UNLIKELY (result == NULL))
    {
      g_warning (G_STRLOC ": Failed to contact HAL: %s", error.message);
      dbus_error_free (&error);
      return FALSE;
    }

  /* pretend that we succeed */
  dbus_message_unref (result);
  return TRUE;
#else
  return FALSE;
#endif
}



XfsmShutdownHelper*
xfsm_shutdown_helper_spawn (void)
{
  XfsmShutdownHelper *helper;
  struct              rlimit rlp;
  gchar               buf[15];
  gint                parent_pipe[2];
  gint                child_pipe[2];
  gint                result;
  gint                n;

  /* allocate a new helper */
  helper = g_new0 (XfsmShutdownHelper, 1);

  /* check if we can use HAL to shutdown the computer */
  if (xfsm_shutdown_helper_hal_check (helper))
    {
      /* well that's it then */
      g_message (G_STRLOC ": Using HAL to shutdown/reboot the computer.");
      helper->use_hal = TRUE;
      return helper;
    }

  /* no HAL, but maybe sudo will do */
  g_message (G_STRLOC ": HAL not available or does not permit to shutdown/reboot the computer, trying sudo fallback instead.");

  /* make sure sudo is installed, and in $PATH */
  helper->sudo = g_find_program_in_path ("sudo");
  if (G_UNLIKELY (helper->sudo == NULL))
    {
      g_warning ("sudo was not found. You will not be able to shutdown your system from within Xfce.");
      g_free (helper);
      return NULL;
    }

  result = pipe (parent_pipe);
  if (result < 0)
    goto error0;

  result = pipe (child_pipe);
  if (result < 0)
    goto error1;

  helper->pid = fork ();
  if (helper->pid < 0)
    {
      goto error2;
    }
  else if (helper->pid == 0)
    {
      /* setup signals */
      signal (SIGPIPE, SIG_IGN);

      /* setup environment */
      xfce_setenv ("LC_ALL", "C", TRUE);
      xfce_setenv ("LANG", "C", TRUE);
      xfce_setenv ("LANGUAGE", "C", TRUE);

      /* setup the 3 standard file handles */
      dup2 (child_pipe[0], STDIN_FILENO);
      dup2 (parent_pipe[1], STDOUT_FILENO);
      dup2 (parent_pipe[1], STDERR_FILENO);

      /* Close all other file handles */
      getrlimit (RLIMIT_NOFILE, &rlp);
      for (n = 0; n < (gint) rlp.rlim_cur; ++n)
        {
          if (n != STDIN_FILENO && n != STDOUT_FILENO && n != STDERR_FILENO)
            close (n);
        }

      /* execute sudo with the helper */
      execl (helper->sudo, "sudo", "-H", "-S", "-p",
             "XFSM_SUDO_PASS ", "--", XFSM_SHUTDOWN_HELPER, NULL);
      _exit (127);
    }

  close (parent_pipe[1]);

  /* read sudo/helper answer */
  n = read (parent_pipe[0], buf, 15);
  if (n < 15)
    goto error2;

  helper->infile = fdopen (parent_pipe[0], "r");
  if (helper->infile == NULL)
    goto error2;

  helper->outfile = fdopen (child_pipe[1], "w");
  if (helper->outfile == NULL)
    goto error3;

  if (memcmp (buf, "XFSM_SUDO_PASS ", 15) == 0)
    {
      helper->need_password = TRUE;
    }
  else if (memcmp (buf, "XFSM_SUDO_DONE ", 15) == 0)
    {
      helper->need_password = FALSE;
    }
  else
    goto error3;

  close (parent_pipe[1]);
  close (child_pipe[0]);

  return helper;

error3:
  if (helper->infile != NULL)
    fclose (helper->infile);
  if (helper->outfile != NULL)
    fclose (helper->outfile);

error2:
  close (child_pipe[0]);
  close (child_pipe[1]);

error1:
  close (parent_pipe[0]);
  close (parent_pipe[1]);

error0:
  g_free (helper);
  return NULL;
}



gboolean
xfsm_shutdown_helper_need_password (const XfsmShutdownHelper *helper)
{
  return helper->need_password;
}



gboolean
xfsm_shutdown_helper_send_password (XfsmShutdownHelper *helper,
                                    const gchar        *password)
{
  gssize result;
  gchar  buffer[1024];
  gsize  failed;
  gsize  length;
  gsize  bytes;
  gint   fd;

  g_return_val_if_fail (helper != NULL, FALSE);
  g_return_val_if_fail (password != NULL, FALSE);
  g_return_val_if_fail (!helper->use_hal, FALSE);
  g_return_val_if_fail (helper->need_password, FALSE);

  g_snprintf (buffer, 1024, "%s\n", password);
  length = strlen (buffer);
  bytes = fwrite (buffer, 1, length, helper->outfile);
  fflush (helper->outfile);
  bzero (buffer, length);

  if (bytes != length)
    {
      fprintf (stderr, "Failed to write password (bytes=%lu, length=%lu)\n", (long)bytes, (long)length);
      return FALSE;
    }

  if (ferror (helper->outfile))
    {
      fprintf (stderr, "Pipe error\n");
      return FALSE;
    }

  fd = fileno (helper->infile);

  for (failed = length = 0;;)
    {
      result = read (fd, buffer + length, 256 - length);

      if (result < 0)
        {
          perror ("read");
          return FALSE;
        }
      else if (result == 0)
        {
          if (++failed > 20)
            return FALSE;
          continue;
        }
      else if (result + length >= 1024)
        {
          fprintf (stderr, "Too much output from sudo!\n");
          return FALSE;
        }

      length += result;
      buffer[length] = 0;

      if (length >= 15)
        {
          if (strncmp (buffer + (length - 15), "XFSM_SUDO_PASS ", 15) == 0)
            {
              return FALSE;
            }
          else if (strncmp (buffer + (length - 15), "XFSM_SUDO_DONE ", 15) == 0)
            {
              helper->need_password = FALSE;
              break;
            }
        } 
    }

  return TRUE;
}



gboolean
xfsm_shutdown_helper_send_command (XfsmShutdownHelper *helper,
                                   XfsmShutdownCommand command)
{
  static gchar *command_table[] = { "POWEROFF", "REBOOT" };
  gchar         response[256];

  g_return_val_if_fail (helper != NULL, FALSE);
  g_return_val_if_fail (!helper->need_password, FALSE);

  /* check if we can use HAL to perform the requested action */
  if (G_LIKELY (helper->use_hal))
    {
      /* well, send the command to HAL then */
      return xfsm_shutdown_helper_hal_send (helper, command);
    }
  else
    {
      /* send it to our associated sudo'ed process */
      fprintf (helper->outfile, "%s\n", command_table[command]);
      fflush (helper->outfile);

      if (ferror (helper->outfile))
        {
          fprintf (stderr, "Error sending command to helper\n");
          return FALSE;
        }

      if (fgets (response, 256, helper->infile) == NULL)
        {
          fprintf (stderr, "No response from helper\n");
          return FALSE;
        }

      if (strncmp (response, "SUCCEED", 7) != 0)
        {
          fprintf (stderr, "Command failed\n");
          return FALSE;
        }
    }

  return TRUE;
}



void
xfsm_shutdown_helper_destroy (XfsmShutdownHelper *helper)
{
  gint status;

  g_return_if_fail (helper != NULL);

  if (helper->infile != NULL)
    fclose (helper->infile);
  if (helper->outfile != NULL)
    fclose (helper->outfile);

  if (helper->pid > 0)
    waitpid (helper->pid, &status, 0);

  g_free (helper->sudo);
  g_free (helper);
}


