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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libxfce4util/libxfce4util.h>

#include "shutdown.h"
#include "xfsm-shutdown-helper.h"

typedef enum
{
    XFSM_SHUTDOWN_SUDO = 0,
    XFSM_SHUTDOWN_HAL,
    XFSM_SHUTDOWN_POWER_MANAGER,
} XfsmShutdownBackend;


static struct
{
  XfsmShutdownType command;
  gchar * name;
} command_name_map[] = {
  { XFSM_SHUTDOWN_HALT,      "Shutdown"  },
  { XFSM_SHUTDOWN_REBOOT,    "Reboot"    },
  { XFSM_SHUTDOWN_SUSPEND,   "Suspend"   },
  { XFSM_SHUTDOWN_HIBERNATE, "Hibernate" },
};


struct _XfsmShutdownHelper
{
  XfsmShutdownBackend backend;

  gchar              *sudo;
  pid_t               pid;
  FILE               *infile;
  FILE               *outfile;
  gboolean            need_password;
};


static DBusConnection *
xfsm_shutdown_helper_dbus_connect (DBusBusType bus_type,
                                   GError **error)
{
  DBusConnection *connection;
  DBusError       derror;

  /* initialize the error */
  dbus_error_init (&derror);

  /* connect to the system message bus */
  connection = dbus_bus_get (bus_type, &derror);
  if (G_UNLIKELY (connection == NULL))
    {
      g_warning (G_STRLOC ": Failed to connect to the system message bus: %s", derror.message);
      if (error)
        dbus_set_g_error (error, &derror);
      dbus_error_free (&derror);
      return NULL;
    }

  return connection;
}



static gboolean
xfsm_shutdown_helper_pm_check (XfsmShutdownHelper *helper)
{
  DBusConnection *connection;
  DBusMessage    *message;
  DBusMessage    *result;
  DBusError       derror;

  connection = xfsm_shutdown_helper_dbus_connect (DBUS_BUS_SESSION, NULL);
  if (!connection)
    return FALSE;

  /* older versions of xfce4-power-manager didn't have the 'CanReboot'
   * method, so we'll key off that one to check for support. */
  message = dbus_message_new_method_call ("org.freedesktop.PowerManagement",
                                          "/org/freedesktop/PowerManagement",
                                          "org.freedesktop.PowerManagement",
                                          "CanReboot");

  dbus_error_init(&derror);
  result = dbus_connection_send_with_reply_and_block (connection, message, -1, &derror);
  dbus_message_unref (message);

  if (result == NULL)
    {
      g_debug (G_STRLOC ": Failed to contact PM: %s", derror.message);
      dbus_error_free (&derror);
      return FALSE;
    }
  else if (dbus_set_error_from_message (&derror, result))
    {
      g_debug (G_STRLOC ": Error talking to PM: %s", derror.message);
      dbus_message_unref (result);
      dbus_error_free (&derror);
      /* PM running, but is broken */
      return FALSE;
    }
  else
    {
      /* it doesn't matter what the response is, but it's valid, so
       * we can use the PM */
      dbus_message_unref (result);
      return TRUE;
    }
}



static gboolean
xfsm_shutdown_helper_pm_send (XfsmShutdownHelper *helper,
                              XfsmShutdownType    command,
                              GError            **error)
{
  DBusConnection *connection;
  const char     *methodname;
  DBusMessage    *message;
  DBusMessage    *result;
  DBusError       derror;
  guint           i;

  for (i = 0; i < G_N_ELEMENTS (command_name_map); i++)
    {
      if (command_name_map[i].command == command)
        {
          methodname = command_name_map[i].name;
          break;
        }
    }

  if (!methodname)
    {
      if (error)
        {
          g_set_error (error, DBUS_GERROR, DBUS_GERROR_INVALID_ARGS,
                       "%s", _("Invalid shutdown type"));
        }
      return FALSE;
    }

  connection = xfsm_shutdown_helper_dbus_connect (DBUS_BUS_SESSION, error);
  if (G_UNLIKELY (!connection))
    return FALSE;

  message = dbus_message_new_method_call ("org.freedesktop.PowerManagement",
                                          "/org/freedesktop/PowerManagement",
                                          "org.freedesktop.PowerManagement",
                                          methodname);

  dbus_error_init (&derror);
  result = dbus_connection_send_with_reply_and_block (connection, message, -1, &derror);
  dbus_message_unref (message);

  if (!result)
    {
      if (error)
        dbus_set_g_error (error, &derror);
      dbus_error_free (&derror);
      return FALSE;
    }
  else if (dbus_set_error_from_message (&derror, result))
    {
      dbus_message_unref (result);
      if (error)
        {
          dbus_set_g_error (error, &derror);
        }
      dbus_error_free (&derror);
      return FALSE;
    }

  return TRUE;
}



static gboolean
xfsm_shutdown_helper_check_pm_cap (const char *capability)
{
  DBusConnection *connection;
  char            method[64];
  DBusMessage    *message;
  DBusMessage    *result;
  DBusError       derror;
  dbus_bool_t     response = FALSE;

  connection = xfsm_shutdown_helper_dbus_connect (DBUS_BUS_SESSION, NULL);
  if (!connection)
    return FALSE;

  g_strlcpy (method, "Can", sizeof (method));
  g_strlcat (method, capability, sizeof (method));

  message = dbus_message_new_method_call ("org.freedesktop.PowerManagement",
                                          "/org/freedesktop/PowerManagement",
                                          "org.freedesktop.PowerManagement",
                                          method);

  dbus_error_init(&derror);
  result = dbus_connection_send_with_reply_and_block (connection, message, -1, &derror);
  dbus_message_unref (message);

  if (G_UNLIKELY (result == NULL))
    {
      g_warning (G_STRLOC ": Failed to contact PM: %s", derror.message);
      dbus_error_free (&derror);
      return FALSE;
    }

  if (!result)
    return FALSE;

  if (dbus_set_error_from_message (&derror, result))
    {
      dbus_error_free (&derror);
      response = FALSE;
    }
  else if (!dbus_message_get_args (result, &derror,
                                   DBUS_TYPE_BOOLEAN, &response,
                                   DBUS_TYPE_INVALID))
    {
      dbus_error_free (&derror);
      response = FALSE;
    }

  dbus_message_unref (result);

  return response;
}



static gboolean
xfsm_shutdown_helper_hal_check (XfsmShutdownHelper *helper)
{
  DBusConnection *connection;
  DBusMessage    *message;
  DBusMessage    *result;
  DBusError       derror;

  g_return_val_if_fail (helper, FALSE);

  connection = xfsm_shutdown_helper_dbus_connect (DBUS_BUS_SYSTEM, NULL);
  if (!connection)
    return FALSE;

  /* initialize the error */
  dbus_error_init (&derror);

  /* this is a simple trick to check whether we are allowed to
   * use the org.freedesktop.Hal.Device.SystemPowerManagement
   * interface without shutting down/rebooting now.
   */
  message = dbus_message_new_method_call ("org.freedesktop.Hal",
                                          "/org/freedesktop/Hal/devices/computer",
                                          "org.freedesktop.Hal.Device.SystemPowerManagement",
                                          "ThisMethodMustNotExistInHal");
  result = dbus_connection_send_with_reply_and_block (connection, message, 2000, &derror);
  dbus_message_unref (message);

  /* translate error results appropriately */
  if (result != NULL && dbus_set_error_from_message (&derror, result))
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
  if (strcmp (derror.name, "org.freedesktop.DBus.Error.UnknownMethod") == 0)
    {
      dbus_error_free (&derror);
      return TRUE;
    }

  /* otherwise, we failed for some reason */
  g_warning (G_STRLOC ": Failed to contact HAL: %s", derror.message);
  dbus_error_free (&derror);

  return FALSE;
}



static gboolean
xfsm_shutdown_helper_hal_send (XfsmShutdownHelper *helper,
                               XfsmShutdownType command,
                               GError **error)
{
  DBusConnection *connection;
  DBusMessage    *message;
  DBusMessage    *result;
  DBusError       derror;
  const gchar    *methodname = NULL;
  dbus_int32_t    wakeup     = 0;
  guint           i;

  for (i = 0; i < G_N_ELEMENTS (command_name_map); i++)
    {
      if (command_name_map[i].command == command)
        {
          methodname = command_name_map[i].name;
          break;
        }
    }

  if (G_UNLIKELY (methodname == NULL))
    {
      if (error)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       _("No HAL method for command %d"), command);
        }
      return FALSE;
    }

  connection = xfsm_shutdown_helper_dbus_connect (DBUS_BUS_SYSTEM, error);
  if(!connection)
    return FALSE;

  /* initialize the error */
  dbus_error_init (&derror);

  /* send the appropriate message to HAL, telling it to shutdown or reboot the system */
  message = dbus_message_new_method_call ("org.freedesktop.Hal",
                                          "/org/freedesktop/Hal/devices/computer",
                                          "org.freedesktop.Hal.Device.SystemPowerManagement",
                                          methodname);

  /* suspend requires additional arguements */
  if (command == XFSM_SHUTDOWN_SUSPEND)
     dbus_message_append_args (message, DBUS_TYPE_INT32, &wakeup, DBUS_TYPE_INVALID);

  result = dbus_connection_send_with_reply_and_block (connection, message, -1, &derror);
  dbus_message_unref (message);

  /* check if we received a result */
  if (G_UNLIKELY (result == NULL))
    {
      g_warning (G_STRLOC ": Failed to contact HAL: %s", derror.message);
      if (error)
        dbus_set_g_error (error, &derror);
      dbus_error_free (&derror);
      return FALSE;
    }

  /* pretend that we succeed */
  dbus_message_unref (result);
  return TRUE;
}



static gboolean
xfsm_shutdown_helper_check_hal_property (const gchar *object_path,
                                         const gchar *property_name)
{
  DBusConnection *connection;
  DBusMessage    *message;
  DBusMessage    *result;
  DBusError       derror;
  dbus_bool_t     response = FALSE;

  connection = xfsm_shutdown_helper_dbus_connect (DBUS_BUS_SYSTEM, NULL);
  if (!connection)
    return FALSE;

  message = dbus_message_new_method_call ("org.freedesktop.Hal",
                                          object_path,
                                          "org.freedesktop.Hal.Device",
                                          "GetPropertyBoolean");
  dbus_message_append_args (message,
                            DBUS_TYPE_STRING, &property_name,
                            DBUS_TYPE_INVALID);

  dbus_error_init(&derror);
  result = dbus_connection_send_with_reply_and_block (connection, message, -1, &derror);
  dbus_message_unref (message);

  if (G_UNLIKELY (result == NULL))
    {
      g_warning (G_STRLOC ": Failed to contact HAL: %s", derror.message);
      dbus_error_free (&derror);
      return FALSE;
    }

  if (!result)
    return FALSE;

  if (dbus_set_error_from_message (&derror, result))
    {
      dbus_error_free (&derror);
      response = FALSE;
    }
  else
    {
      if (!dbus_message_get_args (result, &derror,
                                  DBUS_TYPE_BOOLEAN, &response,
                                  DBUS_TYPE_INVALID))
        {
          dbus_error_free (&derror);
          response = FALSE;
        }
    }

  dbus_message_unref (result);

  return response;
}



XfsmShutdownHelper*
xfsm_shutdown_helper_spawn (GError **error)
{
  XfsmShutdownHelper *helper;
  struct              rlimit rlp;
  gchar               buf[15];
  gint                parent_pipe[2];
  gint                child_pipe[2];
  gint                result;
  gint                n;

  g_return_val_if_fail (!error || !*error, NULL);

  /* allocate a new helper */
  helper = g_new0 (XfsmShutdownHelper, 1);

  /* first choice is the power manager */
  if (xfsm_shutdown_helper_pm_check (helper))
    {
      g_message (G_STRLOC ": Using PM to shutdown/reboot the computer.");
      helper->backend = XFSM_SHUTDOWN_POWER_MANAGER;
      return helper;
    }

  /* check if we can use HAL to shutdown the computer */
  if (xfsm_shutdown_helper_hal_check (helper))
    {
      /* well that's it then */
      g_message (G_STRLOC ": Using HAL to shutdown/reboot the computer.");
      helper->backend = XFSM_SHUTDOWN_HAL;
      return helper;
    }

  /* no HAL, but maybe sudo will do */
  g_message (G_STRLOC ": HAL not available or does not permit to shutdown/reboot the computer, trying sudo fallback instead.");

  /* make sure sudo is installed, and in $PATH */
  helper->sudo = g_find_program_in_path ("sudo");
  if (G_UNLIKELY (helper->sudo == NULL))
    {
      if (error)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       _("Program \"sudo\" was not found.  You will not be able to shutdown your system from within Xfce."));
        }
      g_free (helper);
      return NULL;
    }

  result = pipe (parent_pipe);
  if (result < 0)
    {
      if (error)
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                       _("Unable to create parent pipe: %s"), strerror (errno));
        }
      goto error0;
    }

  result = pipe (child_pipe);
  if (result < 0)
    {
      if (error)
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                       _("Unable to create child pipe: %s"), strerror (errno));
        }
      goto error1;
    }

  helper->pid = fork ();
  if (helper->pid < 0)
    {
      if (error)
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                       _("Unable to fork sudo helper: %s"), strerror (errno));
        }
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
    {
      if (error)
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                       _("Unable to read response from sudo helper: %s"),
                       n < 0 ? strerror (errno) : _("Unknown error"));
        }
      goto error2;
    }

  helper->infile = fdopen (parent_pipe[0], "r");
  if (helper->infile == NULL)
    {
      if (error)
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                       _("Unable to open parent pipe: %s"), strerror (errno));
        }
      goto error2;
    }

  helper->outfile = fdopen (child_pipe[1], "w");
  if (helper->outfile == NULL)
    {
      if (error)
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                       _("Unable to open child pipe: %s"), strerror (errno));
        }
      goto error3;
    }

  if (memcmp (buf, "XFSM_SUDO_PASS ", 15) == 0)
    {
      helper->need_password = TRUE;
    }
  else if (memcmp (buf, "XFSM_SUDO_DONE ", 15) == 0)
    {
      helper->need_password = FALSE;
    }
  else
    {
      if (error)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       _("Got unexpected reply from sudo shutdown helper"));
        }
      goto error3;
    }

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
  g_return_val_if_fail (helper->backend == XFSM_SHUTDOWN_SUDO, FALSE);
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
                                   XfsmShutdownType command,
                                   GError **error)
{
  const gchar *command_str = NULL;
  gchar         response[256];

  g_return_val_if_fail (helper != NULL, FALSE);
  g_return_val_if_fail (!helper->need_password, FALSE);
  g_return_val_if_fail (!error || !*error, FALSE);
  g_return_val_if_fail (command != XFSM_SHUTDOWN_ASK, FALSE);

  if (helper->backend == XFSM_SHUTDOWN_POWER_MANAGER)
    {
        return xfsm_shutdown_helper_pm_send (helper, command, error);
    }
  else if (helper->backend == XFSM_SHUTDOWN_HAL)
    {
      /* well, send the command to HAL then */
      return xfsm_shutdown_helper_hal_send (helper, command, error);
    }
  else
    {
      /* we don't support hibernate or suspend without HAL */
      switch (command)
        {
          case XFSM_SHUTDOWN_HALT:
            command_str = "POWEROFF";
            break;

          case XFSM_SHUTDOWN_REBOOT:
            command_str = "REBOOT";
            break;

          case XFSM_SHUTDOWN_SUSPEND:
          case XFSM_SHUTDOWN_HIBERNATE:
            if (error)
              {
                g_set_error (error, DBUS_GERROR, DBUS_GERROR_SERVICE_UNKNOWN,
                             _("Suspend and Hibernate are only supported through HAL, which is unavailable"));
              }
            /* fall through */
          default:
            return FALSE;
        }

      /* send it to our associated sudo'ed process */
      fprintf (helper->outfile, "%s\n", command_str);
      fflush (helper->outfile);

      if (ferror (helper->outfile))
        {
          if (errno == EINTR)
            {
              /* probably succeeded but the helper got killed */
              return TRUE;
            }

          if (error)
            {
              g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                           _("Error sending command to shutdown helper: %s"),
                           strerror (errno));
            }
          return FALSE;
        }

      if (fgets (response, 256, helper->infile) == NULL)
        {
          if (errno == EINTR)
            {
              /* probably succeeded but the helper got killed */
              return TRUE;
            }

          if (error)
            {
              g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                           _("Error receiving response from shutdown helper: %s"),
                           strerror (errno));
            }
          return FALSE;
        }

      if (strncmp (response, "SUCCEED", 7) != 0)
        {
          if (error)
            {
              g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                           _("Shutdown command failed"));
            }
          return FALSE;
        }
    }

  return TRUE;
}



gboolean
xfsm_shutdown_helper_supports (XfsmShutdownHelper *helper,
                               XfsmShutdownType shutdown_type)
{
  const char *method_name = NULL;
  guint i;

  if (!helper)
    return FALSE;

  switch (helper->backend)
    {
      case XFSM_SHUTDOWN_POWER_MANAGER:
        for (i = 0; i < G_N_ELEMENTS (command_name_map); i++)
          {
            if (command_name_map[i].command == shutdown_type)
              {
                method_name = command_name_map[i].name;
                break;
              }
          }

        if (G_UNLIKELY (!method_name))
          return FALSE;

        return xfsm_shutdown_helper_check_pm_cap (method_name);

      case XFSM_SHUTDOWN_HAL:
        switch (shutdown_type)
          {
            case XFSM_SHUTDOWN_SUSPEND:
              return xfsm_shutdown_helper_check_hal_property ("/org/freedesktop/Hal/devices/computer",
                                                              "power_management.can_suspend");

            case XFSM_SHUTDOWN_HIBERNATE:
              return xfsm_shutdown_helper_check_hal_property ("/org/freedesktop/Hal/devices/computer",
                                                              "power_management.can_hibernate");

            default:
              return TRUE;
          }
        break;

      case XFSM_SHUTDOWN_SUDO:
        switch (shutdown_type)
          {
            case XFSM_SHUTDOWN_SUSPEND:
            case XFSM_SHUTDOWN_HIBERNATE:
              return FALSE;

            default:
              return TRUE;
          }
        break;
    }

  g_assert_not_reached();
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


