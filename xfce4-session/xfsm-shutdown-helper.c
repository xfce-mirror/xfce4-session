/* $Id$ */
/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2010      Ali Abdallah    <aliov@xfce.org>
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

#include "xfsm-shutdown-helper.h"
#include "xfsm-global.h"

static void xfsm_shutdown_helper_finalize     (GObject *object);

static void xfsm_shutdown_helper_set_property (GObject *object,
					       guint prop_id,
					       const GValue *value,
					       GParamSpec *pspec);

static void xfsm_shutdown_helper_get_property (GObject *object,
					       guint prop_id,
					       GValue *value,
					       GParamSpec *pspec);

/**
 * Suspend/Hibernate backend to use.
 *
 * upower, HAL, pmutils via sudo.
 *
 **/
typedef enum
{
  XFSM_SLEEP_BACKEND_NOT_SUPPORTED,
  XFSM_SLEEP_BACKEND_UPOWER,
  XFSM_SLEEP_BACKEND_HAL,
  XFSM_SLEEP_BACKEND_SUDO /*sudo pm-suspend ?*/
 
} XfsmSleepBackend;

/**
 * Shutdown/Reboot backend to use
 *
 * ConsoleKit, HAL, sudo
 **/
typedef enum
{
  XFSM_SHUTDOWN_BACKEND_NOT_SUPPORTED,
  XFSM_SHUTDOWN_BACKEND_CONSOLE_KIT,
  XFSM_SHUTDOWN_BACKEND_HAL,
  XFSM_SHUTDOWN_BACKEND_SUDO

} XfsmShutdownBackend;

#ifdef ENABLE_POLKIT
/*DBus GTypes for polkit calls*/
static GType      polkit_subject_gtype   = G_TYPE_INVALID;
static GType      polkit_details_gtype   = G_TYPE_INVALID;
static GType      polkit_result_gtype    = G_TYPE_INVALID;
#endif /*ENABLE_POLKIT*/

struct _XfsmShutdownHelper
{
  GObject              parent;

  DBusGConnection     *system_bus;

#ifdef ENABLE_POLKIT
  DBusGProxy          *polkit_proxy;
  GValueArray         *polkit_subject;
  GHashTable          *polkit_details;
  GHashTable          *polkit_subject_hash;
#endif

  XfsmShutdownBackend  shutdown_backend;
  XfsmSleepBackend     sleep_backend;
  
  gboolean             auth_shutdown;
  gboolean             auth_restart;
  gboolean             auth_suspend;
  gboolean             auth_hibernate;
  gboolean             can_shutdown;
  gboolean             can_restart;
  gboolean             can_suspend;
  gboolean             can_hibernate;

  gboolean             devkit_is_upower;

  /* Sudo data */
  gchar               *sudo;
  FILE                *infile;
  FILE                *outfile;
  pid_t                pid;
  gboolean             require_password;

};

struct _XfsmShutdownHelperClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_AUTHORIZED_TO_SHUTDOWN,
  PROP_AUTHORIZED_TO_RESTART,
  PROP_AUTHORIZED_TO_SUSPEND,
  PROP_AUTHORIZED_TO_HIBERNATE,
  PROP_CAN_SHUTDOWN,
  PROP_CAN_RESTART,
  PROP_CAN_SUSPEND,
  PROP_CAN_HIBERNATE,
  PROP_USER_CAN_SHUTDOWN,
  PROP_USER_CAN_RESTART,
  PROP_USER_CAN_SUSPEND,
  PROP_USER_CAN_HIBERNATE,
  PROP_REQUIRE_PASSWORD
};

G_DEFINE_TYPE (XfsmShutdownHelper, xfsm_shutdown_helper, G_TYPE_OBJECT)

#ifdef ENABLE_POLKIT

#if defined(__FreeBSD__)
/**
 * Taken from polkitunixprocess.c code to get process start
 * time from pid.
 *
 * Copyright (C) 2008 Red Hat, Inc.
 *
 **/
static gboolean
get_kinfo_proc (pid_t pid, struct kinfo_proc *p)
{
  int mib[4];
  size_t len;

  len = 4;
  sysctlnametomib ("kern.proc.pid", mib, &len);

  len = sizeof (struct kinfo_proc);
  mib[3] = pid;

  if (sysctl (mib, 4, p, &len, NULL, 0) == -1)
    return FALSE;

  return TRUE;
}
#endif /*if defined(__FreeBSD__)*/

static guint64
get_start_time_for_pid (pid_t pid)
{
  guint64 start_time;
#if !defined(__FreeBSD__)
  gchar *filename;
  gchar *contents;
  size_t length;
  gchar **tokens;
  guint num_tokens;
  gchar *p;
  gchar *endp;

  start_time = 0;
  contents = NULL;

  filename = g_strdup_printf ("/proc/%d/stat", pid);

  if (!g_file_get_contents (filename, &contents, &length, NULL))
    goto out;

  /* start time is the token at index 19 after the '(process name)' entry - since only this
   * field can contain the ')' character, search backwards for this to avoid malicious
   * processes trying to fool us
   */
  p = strrchr (contents, ')');
  if (p == NULL)
    {
      goto out;
    }
  p += 2; /* skip ') ' */

  if (p - contents >= (int) length)
    {
      g_warning ("Error parsing file %s", filename);
      goto out;
    }

  tokens = g_strsplit (p, " ", 0);

  num_tokens = g_strv_length (tokens);

  if (num_tokens < 20)
    {
      g_warning ("Error parsing file %s", filename);
      goto out;
    }

  start_time = strtoull (tokens[19], &endp, 10);
  if (endp == tokens[19])
    {
      g_warning ("Error parsing file %s", filename);
      goto out;
    }

  g_strfreev (tokens);

 out:
  g_free (filename);
  g_free (contents);

#else /*if !defined(__FreeBSD__)*/

  struct kinfo_proc p;

  start_time = 0;

  if (! get_kinfo_proc (pid, &p))
    {
      g_warning ("Error obtaining start time for %d (%s)",
		 (gint) pid,
		 g_strerror (errno));
      goto out;
    }

  start_time = (guint64) p.ki_start.tv_sec;

out:
#endif

  return start_time;
}

static void
init_dbus_gtypes (void)
{
  if (G_UNLIKELY (polkit_subject_gtype == G_TYPE_INVALID ) )
    {
      polkit_subject_gtype = 
	dbus_g_type_get_struct ("GValueArray", 
				G_TYPE_STRING, 
				dbus_g_type_get_map ("GHashTable", 
						     G_TYPE_STRING, 
						     G_TYPE_VALUE),
				G_TYPE_INVALID);
    }
  
  if (G_UNLIKELY (polkit_details_gtype == G_TYPE_INVALID ) )
    {
      polkit_details_gtype = dbus_g_type_get_map ("GHashTable", 
						  G_TYPE_STRING, 
						  G_TYPE_STRING);
    }

  if ( G_UNLIKELY (polkit_result_gtype == G_TYPE_INVALID ) )
    {
      polkit_result_gtype =
	dbus_g_type_get_struct ("GValueArray", 
				G_TYPE_BOOLEAN, 
				G_TYPE_BOOLEAN, 
				dbus_g_type_get_map ("GHashTable", 
						     G_TYPE_STRING, 
						     G_TYPE_STRING),
				G_TYPE_INVALID);
    }
}

/**
 * xfsm_shutdown_helper_init_polkit_data:
 *
 * Check for Pokit and
 * Init the neccarry Polkit data an GTypes.
 *
 **/
static gboolean
xfsm_shutdown_helper_init_polkit_data (XfsmShutdownHelper *helper)
{
  const gchar *consolekit_cookie;
  GValue hash_elem = { 0 };
  gboolean subject_created = FALSE;
  
  helper->polkit_proxy = 
    dbus_g_proxy_new_for_name (helper->system_bus,
			       "org.freedesktop.PolicyKit1",
			       "/org/freedesktop/PolicyKit1/Authority",
			       "org.freedesktop.PolicyKit1.Authority");
		
  if ( !helper->polkit_proxy )
    return FALSE;

  /**
   * This variable should be set by the session manager or by 
   * the login manager (gdm?). under clean Xfce environment
   * it is set by the session manager (4.8 and above)  
   * since we don't have a login manager, yet!
   **/
  consolekit_cookie = g_getenv ("XDG_SESSION_COOKIE");
  
  if ( consolekit_cookie )
    {
      DBusGProxy *proxy;
      GError *error = NULL;
      gboolean ret;
      gchar *consolekit_session;
      
      proxy  = dbus_g_proxy_new_for_name (helper->system_bus,
					  "org.freedesktop.ConsoleKit",
					  "/org/freedesktop/ConsoleKit/Manager",
					  "org.freedesktop.ConsoleKit.Manager");
					  
      if ( proxy )
	{
	  ret = dbus_g_proxy_call (proxy, "GetSessionForCookie", &error,
				   G_TYPE_STRING, consolekit_cookie,
				   G_TYPE_INVALID,
				   DBUS_TYPE_G_OBJECT_PATH, &consolekit_session,
				   G_TYPE_INVALID);
	  
	  if ( G_LIKELY (ret) )
	    {
	      GValue val  = { 0 };
	      helper->polkit_subject = g_value_array_new (2);
	      helper->polkit_subject_hash = g_hash_table_new_full (g_str_hash, 
								   g_str_equal, 
								   g_free, 
								   NULL);
	      
	      g_value_init (&val, G_TYPE_STRING);
	      g_value_set_string (&val, "unix-session");
	      g_value_array_append (helper->polkit_subject, &val);
	      
	      g_value_unset (&val);
	      g_value_init (&val, G_TYPE_STRING);
	      g_value_set_string (&val, consolekit_session);
	      
	      g_hash_table_insert (helper->polkit_subject_hash, g_strdup ("session-id"), &val);
	      
	      g_free (consolekit_session);
	      subject_created = TRUE;
	    }
	  else if (error)
	    {
	      g_warning ("'GetSessionForCookie' failed : %s", error->message);
	      g_error_free (error);
	    }
	  g_object_unref (proxy);
	}
    }
  
  /**
   * We failed to get valid session data, then we try
   * to check authentication using the pid.
   **/
  if ( subject_created == FALSE )
    {
      gint pid;
      guint64 start_time;

      pid = getpid ();
      
      start_time = get_start_time_for_pid (pid);

      if ( G_LIKELY (start_time != 0 ) )
	{
	  GValue val = { 0 }, pid_val = { 0 }, start_time_val = { 0 };
	  
	  helper->polkit_subject = g_value_array_new (2);
	  helper->polkit_subject_hash = g_hash_table_new_full (g_str_hash, 
							       g_str_equal, 
							       g_free, 
							       NULL);

	  g_value_init (&val, G_TYPE_STRING);
	  g_value_set_string (&val, "unix-process");
	  g_value_array_append (helper->polkit_subject, &val);

	  g_value_unset (&val);
	  
	  g_value_init (&pid_val, G_TYPE_UINT);
	  g_value_set_uint (&pid_val, pid);
	  g_hash_table_insert (helper->polkit_subject_hash, g_strdup ("pid"), &pid_val);

	  g_value_init (&start_time_val, G_TYPE_UINT64);
	  g_value_set_uint64 (&start_time_val, start_time);
	  g_hash_table_insert (helper->polkit_subject_hash, g_strdup ("start-time"), &start_time_val);
	}
      else
	{
	  g_warning ("Unable to create Polkit subject");
	  return FALSE;
	}
    }

  g_value_init (&hash_elem, 
		dbus_g_type_get_map ("GHashTable", 
				     G_TYPE_STRING, 
				     G_TYPE_VALUE));
  
  g_value_set_static_boxed (&hash_elem, helper->polkit_subject_hash);
  g_value_array_append (helper->polkit_subject, &hash_elem);
  
  /**
   * Polkit details, will leave it empty.
   **/
  helper->polkit_details = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  return TRUE;
}
#endif /*ENABLE_POLKIT*/

/**
 * xfsm_shutdown_helper_check_sudo:
 * 
 * Check if we can use sudo backend.
 * Method is called if HAL, upower, consolekit are missing
 * or the session was compiled without those services.
 * 
 **/
static gboolean
xfsm_shutdown_helper_check_sudo (XfsmShutdownHelper *helper)
{
  struct  rlimit rlp;
  gchar   buf[15];
  gint    parent_pipe[2];
  gint    child_pipe[2];
  gint    result;
  gint    n;
  
  helper->sudo = g_find_program_in_path ("sudo");
  
  if ( G_UNLIKELY (helper->sudo == NULL ) )
    {
      g_warning ("Program 'sudo' was not found.");
      return FALSE;
    }
  
  result = pipe (parent_pipe);
  
  if (result < 0)
    {
      g_warning ("Unable to create parent pipe: %s", strerror (errno));
      return FALSE;
    }

  result = pipe (child_pipe);
  if (result < 0)
    {
      g_warning ("Unable to create child pipe: %s", strerror (errno));
      goto error0;
    }

  helper->pid = fork ();
  
  if (helper->pid < 0)
    {
      g_warning ("Unable to fork sudo helper: %s", strerror (errno));
      goto error1;
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
             "XFSM_SUDO_PASS ", "--", XFSM_SHUTDOWN_HELPER_CMD, NULL);
      _exit (127);
    }
  
  close (parent_pipe[1]);
  
  /* read sudo/helper answer */
  n = read (parent_pipe[0], buf, 15);
  if (n < 15)
    {
      g_warning ("Unable to read response from sudo helper: %s",
		 n < 0 ? strerror (errno) : "Unknown error");
      goto error1;
    }

  helper->infile = fdopen (parent_pipe[0], "r");
  
  if (helper->infile == NULL)
    {
      g_warning ("Unable to open parent pipe: %s", strerror (errno));
      goto error1;
    }

  helper->outfile = fdopen (child_pipe[1], "w");

  if (helper->outfile == NULL)
    {
      g_warning ("Unable to open child pipe: %s", strerror (errno));
      goto error2;
    }

  if (memcmp (buf, "XFSM_SUDO_PASS ", 15) == 0)
    {
      helper->require_password = TRUE;
    }
  else if (memcmp (buf, "XFSM_SUDO_DONE ", 15) == 0)
    {
      helper->require_password = FALSE;
    }
  else
    {
      g_warning ("Got unexpected reply from sudo shutdown helper");
      goto error2;
    }

  close (parent_pipe[1]);
  close (child_pipe[0]);

  return TRUE;

error2:
  if (helper->infile != NULL)
    {
      fclose (helper->infile);
      helper->infile = NULL;
    }
  
  if (helper->outfile != NULL)
    {
      fclose (helper->outfile);
      helper->outfile = NULL;
    }

error1:
  close (child_pipe[0]);
  close (child_pipe[1]);

error0:
  close (parent_pipe[0]);
  close (parent_pipe[1]);

  return FALSE;
}

#ifdef ENABLE_UPOWER
/**
 * xfsm_shutdown_helper_get_power_props:
 *
 **/
static GHashTable *
xfsm_shutdown_helper_get_power_props (XfsmShutdownHelper *helper, 
				      const gchar *name, 
				      const gchar *path,
				      const gchar *iface)
{
  DBusGProxy *proxy_prop;
  GHashTable *props;
  GType g_type_hash_map;
  GError *error = NULL;

  proxy_prop = dbus_g_proxy_new_for_name (helper->system_bus,
					  name,
					  path,
					  DBUS_INTERFACE_PROPERTIES);

  if ( !proxy_prop )
    {
      g_warning ("Unable to create proxy for %s", name);
      return NULL;
    }

  /* The Hash table is a pair of (strings, GValues) */
  g_type_hash_map = dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE);

  dbus_g_proxy_call (proxy_prop, "GetAll", &error,
		     G_TYPE_STRING, iface,
		     G_TYPE_INVALID,
		     g_type_hash_map, &props,
		     G_TYPE_INVALID);

  g_object_unref (proxy_prop);

  if ( error )
    {
      g_warning ("Method 'GetAll' failed : %s", error->message);
      g_error_free (error);
      return NULL;
    }

  return props;
}


/**
 * xfsm_shutdown_helper_check_devkit_upower:
 * 
 * Check upower (formely devicekit-power)
 * for hibernate and suspend availability.
 * 
 * Returns: FALSE if failed to contact upower, TRUE otherwise.
 **/
static gboolean
xfsm_shutdown_helper_check_devkit_upower (XfsmShutdownHelper *helper)
{
  /**
   * Get the properties on 'org.freedesktop.UPower'
   *                    or 'org.freedesktop.DeviceKit.Power'
   *
   * DaemonVersion      's'
   * CanSuspend'        'b'
   * CanHibernate'      'b'
   * OnBattery'         'b'
   * OnLowBattery'      'b'
   * LidIsClosed'       'b'
   * LidIsPresent'      'b'
   **/

  GHashTable *props;
  GValue *value;
  const gchar *name, *path, *iface;
  
  /* Check for upower first */
  name = "org.freedesktop.UPower";
  path = "/org/freedesktop/UPower";
  iface = "org.freedesktop.UPower";

  helper->devkit_is_upower = TRUE;

  props = xfsm_shutdown_helper_get_power_props (helper,
						name,
						path,
						iface);
  if ( !props )
    {
      g_message ("UPower not found, trying DevKitPower");

      name = "org.freedesktop.DeviceKit.Power";
      path = "/org/freedesktop/DeviceKit/Power";
      iface = "org.freedesktop.DeviceKit.Power";

      helper->devkit_is_upower = FALSE;

      props = xfsm_shutdown_helper_get_power_props (helper,
						    name,
						    path,
						    iface);
      if (!props)
	{
	  g_message ("Devkit Power is not running or not installed");
	  return FALSE;
	}
    }
  
  /*Get The CanSuspend bit*/
  value = g_hash_table_lookup (props, "CanSuspend");
  
  if ( G_LIKELY (value) )
    {
      helper->can_suspend = g_value_get_boolean (value);
    }
  else
    {
      g_warning ("No 'CanSuspend' property");
    }
  
  /*Get the CanHibernate bit*/
  value = g_hash_table_lookup (props, "CanHibernate");
  
  if ( G_LIKELY (value) ) 
    {
      helper->can_hibernate = g_value_get_boolean (value);
    }
  else
    {
      g_warning ("No 'CanHibernate' property");
    }
  
  g_hash_table_destroy (props);

  return TRUE;
}
#endif /*ENABLE_UPOWER*/


/**
 * xfsm_shutdown_helper_check_console_kit:
 *
 * Check Consolekit for methods:
 * Stop (Shutdown), Restart.
 **/
#ifdef ENABLE_CONSOLE_KIT
static gboolean
xfsm_shutdown_helper_check_console_kit (XfsmShutdownHelper *helper)
{
  DBusGProxy *proxy;
  GError *error = NULL;
  gboolean ret;

  proxy = dbus_g_proxy_new_for_name (helper->system_bus,
				     "org.freedesktop.ConsoleKit",
				     "/org/freedesktop/ConsoleKit/Manager",
				     "org.freedesktop.ConsoleKit.Manager");
				     

  if (!proxy)
    {
      g_warning ("Failed to create proxy for 'org.freedesktop.ConsoleKit'");
      return FALSE;
    }

  ret = dbus_g_proxy_call (proxy, "CanStop", &error,
			   G_TYPE_INVALID,
			   G_TYPE_BOOLEAN, &helper->can_shutdown,
			   G_TYPE_INVALID);
  
  if ( error )
    {
      g_warning ("'CanStop' method failed : %s", error->message);
      g_error_free (error);
      goto out;
    }
  
  dbus_g_proxy_call (proxy, "CanRestart", &error,
		     G_TYPE_INVALID,
		     G_TYPE_BOOLEAN, &helper->can_restart,
		     G_TYPE_INVALID);
  
  if ( error )
    {
      g_warning ("'CanRestart' method failed : %s", error->message);
      g_error_free (error);
      goto out;
    }
    
  ret = TRUE;

 out:

  g_object_unref (proxy);

  return ret;
}
#endif /*ENABLE_CONSOLE_KIT*/


/**
 * xfsm_shutdown_helper_check_hal:
 *
 * Check if HAL is running and for its PowerManagement bits
 * 
 **/
#ifdef ENABLE_HAL
static gboolean
xfsm_shutdown_helper_check_hal (XfsmShutdownHelper *helper)
{
  DBusGProxy *proxy_power;
  DBusGProxy *proxy_device;
  GError *error = NULL;
  gboolean ret = FALSE;

  proxy_power = 
      dbus_g_proxy_new_for_name (helper->system_bus,
				 "org.freedesktop.Hal",
				 "/org/freedesktop/Hal/devices/computer",
				 "org.freedesktop.Hal.Device.SystemPowerManagement");
  
  if (!proxy_power)
    {
      g_warning ("Failed to create proxy for 'org.freedesktop.Hal' ");
      return FALSE;
    }
		
  dbus_g_proxy_call (proxy_power, "JustToCheckUserPermission", &error,
		     G_TYPE_INVALID,
		     G_TYPE_INVALID);
  
  g_object_unref (proxy_power);

  if ( G_LIKELY (error) )
    {
      /* The message passed dbus permission check? */
      if ( !g_error_matches (error, DBUS_GERROR, DBUS_GERROR_UNKNOWN_METHOD) )
	{
	  g_error_free (error);
	  return FALSE;
	}
      g_error_free (error);
      error = NULL;
    }
  else
    {
      /*Something went wrong with HAL*/
      return FALSE;
    }

  /*
   * Message raised DBUS_GERROR_UNKNOWN_METHOD, so it reached HAL,
   * we can consider thatn  HAL allows us to use its power management interface
   */
  helper->auth_shutdown  = TRUE;
  helper->auth_restart   = TRUE;
  helper->auth_suspend   = TRUE;
  helper->auth_hibernate = TRUE;
  /* HAL doesn't have can_shutdown and can_reboot bits since it can always
   shutdown and reboot the system */
  helper->can_shutdown   = TRUE;
  helper->can_restart    = TRUE;

  /*Check to see is the system can_hibernate and can_suspend*/
  proxy_device = dbus_g_proxy_new_for_name (helper->system_bus,
					    "org.freedesktop.Hal",
					    "/org/freedesktop/Hal/devices/computer",
					    "org.freedesktop.Hal.Device");


  if ( !proxy_device )
      return FALSE;

  dbus_g_proxy_call (proxy_device, "GetPropertyBoolean", &error,
		     G_TYPE_STRING, "power_management.can_hibernate",
		     G_TYPE_INVALID,
		     G_TYPE_BOOLEAN, &helper->can_hibernate,
		     G_TYPE_INVALID);

  if ( error )
    {
      g_warning ("Method 'GetPropertyBoolean' failed : %s", error->message);
      g_error_free (error);
      goto out;
    }

  dbus_g_proxy_call (proxy_device, "GetPropertyBoolean", &error,
		     G_TYPE_STRING, "power_management.can_suspend",
		     G_TYPE_INVALID,
		     G_TYPE_BOOLEAN, &helper->can_suspend,
		     G_TYPE_INVALID);
  
  if ( error )
    {
      g_warning ("Method 'GetPropertyBoolean' failed : %s", error->message);
      g_error_free (error);
      goto out;
    }

  ret = TRUE;

 out:
  
  g_object_unref (proxy_device);
  
  return ret;
}
#endif /*ENABLE_HAL*/

/**
 * xfsm_shutdown_helper_check_polkit_authorization:
 *
 * 
 **/
#ifdef ENABLE_POLKIT
static gboolean
xfsm_shutdown_helper_check_authorization (XfsmShutdownHelper *helper,
					  const gchar *action_id)
{
  GValueArray *result;
  GValue result_val = { 0 };
  GError *error = NULL;
  gboolean is_authorized = FALSE;
  gboolean ret;
  
  /**
   * <method name="CheckAuthorization">      
   *   <arg type="(sa{sv})" name="subject" direction="in"/>      
   *   <arg type="s" name="action_id" direction="in"/>           
   *   <arg type="a{ss}" name="details" direction="in"/>         
   *   <arg type="u" name="flags" direction="in"/>               
   *   <arg type="s" name="cancellation_id" direction="in"/>     
   *   <arg type="(bba{ss})" name="result" direction="out"/>     
   * </method>
   *
   **/

  g_return_val_if_fail (helper->polkit_proxy != NULL, FALSE);
  g_return_val_if_fail (helper->polkit_subject, FALSE);
  g_return_val_if_fail (helper->polkit_details, FALSE);

  /* Initialized DBus GTypes */
  init_dbus_gtypes ();

  result = g_value_array_new (0);
  
  ret = dbus_g_proxy_call (helper->polkit_proxy, "CheckAuthorization", &error,
			   polkit_subject_gtype, helper->polkit_subject,
			   G_TYPE_STRING, action_id,
			   polkit_details_gtype, helper->polkit_details,
			   G_TYPE_UINT, 0, 
			   G_TYPE_STRING, NULL,
			   G_TYPE_INVALID,
			   polkit_result_gtype, &result,
			   G_TYPE_INVALID);

  if ( G_LIKELY (ret) )
    {
      g_value_init (&result_val, polkit_result_gtype);
      g_value_set_static_boxed (&result_val, result);

      dbus_g_type_struct_get (&result_val,
			      0, &is_authorized,
			      G_MAXUINT);
      g_value_unset (&result_val);
    }
  else if ( error )
    {
      g_warning ("'CheckAuthorization' failed : %s", error->message);
      g_error_free (error);
  
    }

  g_value_array_free (result);
  return is_authorized;
}
#endif /*ENABLE_POLKIT*/

/**
 *xfsm_shutdown_helper_check_backends:
 * 
 * Try to check what is the best available backend to use
 * Supported: ConsoleKit, HAL, upower and sudo.
 *
 **/
static void
xfsm_shutdown_helper_check_backends (XfsmShutdownHelper *helper)
{
#ifdef ENABLE_POLKIT
  /*if polkit data was successfully initialized*/
  gboolean polkit_ok = FALSE;
#endif

#ifdef ENABLE_UPOWER
  /*Check upower (formely devicekit-power)*/
  if ( xfsm_shutdown_helper_check_devkit_upower (helper) )
    helper->sleep_backend = XFSM_SLEEP_BACKEND_UPOWER;
#endif /* ENABLE_UPOWER */

#ifdef ENABLE_CONSOLE_KIT
  if ( xfsm_shutdown_helper_check_console_kit (helper) )
    {
      helper->shutdown_backend = XFSM_SHUTDOWN_BACKEND_CONSOLE_KIT;
      /*ConsoleKit doesn't use Polkit*/
      helper->auth_shutdown = helper->can_shutdown;
      helper->auth_restart  = helper->can_restart;
    }
#endif

  if ( helper->sleep_backend  == XFSM_SLEEP_BACKEND_UPOWER )
    {
#ifdef ENABLE_POLKIT
      
      polkit_ok = xfsm_shutdown_helper_init_polkit_data (helper);
      
      if ( polkit_ok )
	{
	  const gchar *action_hibernate, *action_suspend;
	  
	  if ( helper->devkit_is_upower )
	    {
	      action_hibernate = "org.freedesktop.upower.suspend";
	      action_suspend   = "org.freedesktop.upower.hibernate";
	    }
	  else
	    {
	      action_hibernate = "org.freedesktop.devicekit.power.hibernate";
	      action_suspend   = "org.freedesktop.devicekit.power.suspend";
	    }
      
	  helper->auth_hibernate =
	    xfsm_shutdown_helper_check_authorization (helper,
						      action_hibernate);
	  helper->auth_suspend =
	    xfsm_shutdown_helper_check_authorization (helper,
						      action_suspend);
	}
      else
	{
	  helper->auth_hibernate = TRUE;
	  helper->auth_suspend   = TRUE;
	}
#else
      helper->auth_hibernate = TRUE;
      helper->auth_suspend   = TRUE;
#endif /* ENABLE_POLKIT */
    }

  /* 
   * Use HAL just as a fallback.
   */
#ifdef ENABLE_HAL
  if ( helper->sleep_backend == XFSM_SLEEP_BACKEND_NOT_SUPPORTED ||
       helper->shutdown_backend == XFSM_SHUTDOWN_BACKEND_NOT_SUPPORTED )
    {
      if ( xfsm_shutdown_helper_check_hal (helper) )
	{
	  if ( helper->shutdown_backend == XFSM_SHUTDOWN_BACKEND_NOT_SUPPORTED )
	    helper->shutdown_backend = XFSM_SHUTDOWN_BACKEND_HAL;
	  
	  if ( helper->sleep_backend == XFSM_SLEEP_BACKEND_NOT_SUPPORTED )
	    helper->sleep_backend    = XFSM_SLEEP_BACKEND_HAL;
	}
    }
#endif

  /* Fallback for sudo */
  if ( helper->shutdown_backend == XFSM_SHUTDOWN_BACKEND_NOT_SUPPORTED )
    {
      if (xfsm_shutdown_helper_check_sudo (helper) )
	{
	  helper->shutdown_backend = XFSM_SHUTDOWN_BACKEND_SUDO;
	  helper->can_shutdown  = TRUE;
	  helper->can_restart   =  TRUE;
	  helper->auth_shutdown = TRUE;
	  helper->auth_restart  = TRUE;
	}
    }

#ifdef DEBUG
  {
    const gchar *shutdown_str[] =  {"Shutdown not supported",
				    "Using ConsoleKit for shutdown",
				    "Using HAL for shutdown",
				    "Using Sudo for shutdown",
				    NULL};
    
    const gchar *sleep_str[] = {"Sleep not supported",
				"Using UPower for sleep",
				"Using HAL for sleep",
				"Using Sudo for sleep",
				NULL};

    g_debug ("%s", shutdown_str[helper->shutdown_backend]);
    g_debug ("%s", sleep_str[helper->sleep_backend]);

    g_debug ("can_shutdown=%d can_restart=%d can_hibernate=%d can_suspend=%d",
	     helper->can_shutdown, helper->can_restart, 
	     helper->can_hibernate, helper->can_suspend);
    
    g_debug ("auth_shutdown=%d auth_restart=%d auth_hibernate=%d auth_suspend=%d",
	     helper->auth_shutdown, helper->auth_restart,
	     helper->auth_hibernate, helper->auth_suspend);
  }
#endif

#ifdef ENABLE_POLKIT

  if ( polkit_ok )
    {
      g_hash_table_destroy (helper->polkit_details);
      g_hash_table_destroy (helper->polkit_subject_hash);
      g_value_array_free (helper->polkit_subject);
    }

#endif

}

static void
xfsm_shutdown_helper_class_init (XfsmShutdownHelperClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  
  object_class->finalize = xfsm_shutdown_helper_finalize;
    
  object_class->get_property = xfsm_shutdown_helper_get_property;
  object_class->set_property = xfsm_shutdown_helper_set_property;

  /**
   * XfsmShutdownHelper::authorized-to-shutdown
   * 
   * Whether the user is allowed to preform shutdown.
   **/
  g_object_class_install_property (object_class,
				   PROP_AUTHORIZED_TO_SHUTDOWN,
				   g_param_spec_boolean ("authorized-to-shutdown",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));

  /**
   * XfsmShutdownHelper::authorized-to-reboot
   * 
   * Whether the user is allowed to preform reboot.
   **/
  g_object_class_install_property (object_class,
				   PROP_AUTHORIZED_TO_RESTART,
				   g_param_spec_boolean ("authorized-to-restart",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));

  /**
   * XfsmShutdownHelper::authorized-to-suspend
   * 
   * Whether the user is allowed to preform suspend.
   **/
  g_object_class_install_property (object_class,
				   PROP_AUTHORIZED_TO_SUSPEND,
				   g_param_spec_boolean ("authorized-to-suspend",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));

  /**
   * XfsmShutdownHelper::authorized-to-hibernate
   * 
   * Whether the user is allowed to preform hibernate.
   **/
  g_object_class_install_property (object_class,
				   PROP_AUTHORIZED_TO_HIBERNATE,
				   g_param_spec_boolean ("authorized-to-hibernate",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));
  
  /**
   * XfsmShutdownHelper::can-shutdown
   * 
   * Whether the system can do shutdown.
   **/
  g_object_class_install_property (object_class,
				   PROP_CAN_SHUTDOWN,
				   g_param_spec_boolean ("can-shutdown",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));

  /**
   * XfsmShutdownHelper::can-restart
   * 
   * Whether the system can do restart.
   **/
  g_object_class_install_property (object_class,
				   PROP_CAN_RESTART,
				   g_param_spec_boolean ("can-restart",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));

  /**
   * XfsmShutdownHelper::can-suspend
   * 
   * Whether the system can do suspend.
   **/
  g_object_class_install_property (object_class,
				   PROP_CAN_SUSPEND,
				   g_param_spec_boolean ("can-suspend",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));
  /**
   * XfsmShutdownHelper::can-hibernate
   * 
   * Whether the system can do hibernate.
   **/
  g_object_class_install_property (object_class,
				   PROP_CAN_HIBERNATE,
				   g_param_spec_boolean ("can-hibernate",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));

  /**
   * XfsmShutdownHelper::user-can-shutdown
   * 
   * Whether the user is allowed and the system can shutdown.
   **/
  g_object_class_install_property (object_class,
				   PROP_USER_CAN_SHUTDOWN,
				   g_param_spec_boolean ("user-can-shutdown",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));

  /**
   * XfsmShutdownHelper::user-can-restart
   * 
   * Whether the user is allowed and the system can restart.
   **/
  g_object_class_install_property (object_class,
				   PROP_USER_CAN_RESTART,
				   g_param_spec_boolean ("user-can-restart",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));

  /**
   * XfsmShutdownHelper::user-can-suspend
   * 
   * Whether the user is allowed and the system can suspend.
   **/
  g_object_class_install_property (object_class,
				   PROP_USER_CAN_SUSPEND,
				   g_param_spec_boolean ("user-can-suspend",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));
  /**
   * XfsmShutdownHelper::user-can-hibernate
   * 
   * Whether the user is allowed and the system can hibernate.
   **/
  g_object_class_install_property (object_class,
				   PROP_USER_CAN_HIBERNATE,
				   g_param_spec_boolean ("user-can-hibernate",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));

  /**
   * XfsmShutdownHelper::require-password
   * 
   * Whether the shutdown operation requires a password to
   * pass to sudo.
   **/
  g_object_class_install_property (object_class,
				   PROP_REQUIRE_PASSWORD,
				   g_param_spec_boolean ("require-password",
							 NULL, NULL,
							 FALSE,
							 G_PARAM_READABLE));

}

static void
xfsm_shutdown_helper_init (XfsmShutdownHelper *helper)
{
  GError *error = NULL;

#ifdef ENABLE_POLKIT
  helper->polkit_proxy     = NULL;
  helper->polkit_subject   = NULL;
  helper->polkit_details   = NULL;
  helper->polkit_subject_hash = NULL;
#endif

  helper->sudo             = NULL;
  helper->infile           = NULL;
  helper->outfile          = NULL;
  helper->pid              = 0;
  helper->require_password = FALSE;

  helper->sleep_backend    = XFSM_SLEEP_BACKEND_NOT_SUPPORTED;
  helper->shutdown_backend = XFSM_SHUTDOWN_BACKEND_NOT_SUPPORTED;

  helper->auth_shutdown    = FALSE;
  helper->auth_restart     = FALSE;
  helper->auth_suspend     = FALSE;
  helper->auth_hibernate   = FALSE;
  helper->can_shutdown     = FALSE;
  helper->can_restart      = FALSE;
  helper->can_suspend      = FALSE;
  helper->can_hibernate    = FALSE;

  helper->system_bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);

  if ( G_LIKELY (helper->system_bus) )
    {
      xfsm_shutdown_helper_check_backends (helper);
    }
  else
    /* Unable to connect to the system bus, just try sudo*/
    {
      g_critical ("Failed to connect to the system bus : %s", error->message);
      g_error_free (error);
      if ( xfsm_shutdown_helper_check_sudo (helper) )
	helper->shutdown_backend = XFSM_SHUTDOWN_BACKEND_SUDO;
    }
}

static void xfsm_shutdown_helper_set_property (GObject *object,
					       guint prop_id,
					       const GValue *value,
					       GParamSpec *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void xfsm_shutdown_helper_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
  XfsmShutdownHelper *helper;

  helper = XFSM_SHUTDOWN_HELPER (object);
  
  switch (prop_id)
    {
    case PROP_AUTHORIZED_TO_SHUTDOWN:
      g_value_set_boolean (value, helper->auth_shutdown);
      break;
    case PROP_AUTHORIZED_TO_RESTART:
      g_value_set_boolean (value, helper->auth_restart);
      break;
    case PROP_AUTHORIZED_TO_SUSPEND:
      g_value_set_boolean (value, helper->auth_suspend);
      break;
    case PROP_AUTHORIZED_TO_HIBERNATE:
      g_value_set_boolean (value, helper->auth_hibernate);
      break;
    case PROP_CAN_SUSPEND:
      g_value_set_boolean (value, helper->can_suspend);
      break;
    case PROP_CAN_HIBERNATE:
      g_value_set_boolean (value, helper->can_hibernate);
      break;
    case PROP_CAN_RESTART:
      g_value_set_boolean (value, helper->can_restart);
      break;
    case PROP_CAN_SHUTDOWN:
      g_value_set_boolean (value, helper->can_shutdown);
      break;
    case PROP_USER_CAN_SUSPEND:
      g_value_set_boolean (value, helper->can_suspend && helper->auth_suspend);
      break;
    case PROP_USER_CAN_HIBERNATE:
      g_value_set_boolean (value, helper->can_hibernate && helper->auth_hibernate);
      break;
    case PROP_USER_CAN_RESTART:
      g_value_set_boolean (value, helper->can_restart && helper->auth_restart);
      break;
    case PROP_USER_CAN_SHUTDOWN:
      g_value_set_boolean (value, helper->can_shutdown && helper->auth_shutdown);
      break;
    case PROP_REQUIRE_PASSWORD:
      g_value_set_boolean (value, helper->require_password);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
xfsm_shutdown_helper_finalize (GObject *object)
{
  XfsmShutdownHelper *helper;
  gint status;

  helper = XFSM_SHUTDOWN_HELPER (object);

#ifdef ENABLE_POLKIT
  if ( helper->polkit_proxy )
    g_object_unref (helper->polkit_proxy);
#endif

  if ( helper->system_bus )
    dbus_g_connection_unref (helper->system_bus);
  
  if (helper->infile != NULL)
    fclose (helper->infile);
  
  if (helper->outfile != NULL)
    fclose (helper->outfile);
  
  if (helper->pid > 0)
    waitpid (helper->pid, &status, 0);
      
  g_free (helper->sudo);

  G_OBJECT_CLASS (xfsm_shutdown_helper_parent_class)->finalize (object);
}

/**
 * xfsm_shutdown_helper_upower_sleep:
 * @helper: a #XfsmShutdownHelper,
 * @action: 'Hibernate' or 'Suspend'
 * @error: a #GError
 *
 * 
 **/
#ifdef ENABLE_UPOWER
static gboolean
xfsm_shutdown_helper_upower_sleep (XfsmShutdownHelper *helper,
				   const gchar *action,
				   GError **error)
{
  DBusGProxy *proxy;
  GError *error_local = NULL;
  const gchar *name, *path, *iface;
  gboolean ret;

  if ( helper->devkit_is_upower )
    {
      name = "org.freedesktop.UPower";
      path = "/org/freedesktop/UPower";
      iface = "org.freedesktop.UPower";
    }
  else
    {
      name = "org.freedesktop.DeviceKit.Power";
      path = "/org/freedesktop/DeviceKit/Power";
      iface = "org.freedesktop.DeviceKit.Power";
    }
  
  g_message (G_STRLOC ": Using %s to %s", name, action);

  proxy = dbus_g_proxy_new_for_name_owner (helper->system_bus,
					   name,
					   path,
					   iface,
					   &error_local);
				     
  if ( !proxy )
    {
      g_warning ("Failed to create proxy for %s : %s", name, error_local->message);
      g_set_error (error, 1, 0, "%s", error_local->message);
      g_error_free (error_local);
      return FALSE;
    }
  
  ret = dbus_g_proxy_call (proxy, action, &error_local,
			   G_TYPE_INVALID,
			   G_TYPE_INVALID);

  g_object_unref (proxy);

  if ( !ret )
    {
      /* DBus timeout?*/
      if ( g_error_matches (error_local, DBUS_GERROR, DBUS_GERROR_NO_REPLY) )
        {
	  g_error_free (error_local);
	  return TRUE;
        }
      else
	{
	  g_set_error (error, 1, 0, "%s", error_local->message);
	  g_error_free (error_local);
	  return FALSE;
	}
    }
  
  return TRUE;
}
#endif /*ENABLE_UPOWER*/

/**
 * xfsm_shutdown_helper_console_kit_shutdown:
 * @helper: a #XfsmShutdownHelper,
 * @action: 'Stop' for shutdown and 'Restart' for reboot.
 * @error: a #GError
 *
 * 
 **/
#ifdef ENABLE_CONSOLE_KIT
static gboolean
xfsm_shutdown_helper_console_kit_shutdown (XfsmShutdownHelper *helper, 
					   const gchar *action,
					   GError **error)
{
  DBusGProxy *proxy;
  GError *error_local = NULL;
  gboolean ret;

  g_message (G_STRLOC ": Using ConsoleKit to %s", action);
  
  proxy = dbus_g_proxy_new_for_name_owner (helper->system_bus,
					   "org.freedesktop.ConsoleKit",
					   "/org/freedesktop/ConsoleKit/Manager",
					   "org.freedesktop.ConsoleKit.Manager",
					   &error_local);
  
  if (!proxy)
    {
      g_warning ("Failed to create proxy for 'org.freedesktop.ConsoleKit' : %s", error_local->message);
      g_set_error (error, 1, 0, "%s", error_local->message);
      g_error_free (error_local);
      return FALSE;
    }
  
  ret = dbus_g_proxy_call (proxy, action, &error_local,
			   G_TYPE_INVALID,
			   G_TYPE_INVALID);
  
  g_object_unref (proxy);
  
  if ( !ret )
    {
      g_set_error (error, 1, 0, "%s", error_local->message);
      g_error_free (error_local);
      return FALSE;
    }
  
  return TRUE;
}
#endif /*ENABLE_CONSOLE_KIT*/

/**
 * xfsm_shutdown_helper_hal_send:
 * @helper: a #XfsmShutdownHelper,
 * @action: 'Reboot' 'Shutdown' 'Hibernate' 'Suspend'
 * @error: a #GError
 *
 **/
#ifdef ENABLE_HAL
static gboolean
xfsm_shutdown_helper_hal_send (XfsmShutdownHelper *helper,
			       const gchar *action,
			       GError **error)
{
  DBusGProxy *proxy;
  GError *error_local = NULL;
  gboolean ret;
  gint return_code;
  
  g_message (G_STRLOC ": Using ConsoleKit to %s", action);

  proxy = dbus_g_proxy_new_for_name_owner (helper->system_bus,
					   "org.freedesktop.Hal",
					   "/org/freedesktop/Hal/devices/computer",
					   "org.freedesktop.Hal.Device.SystemPowerManagement",
					   &error_local);
  if ( !proxy )
    {
      g_warning ("Failed to create proxy for 'org.freedesktop.Hal' : %s", error_local->message);
      g_set_error (error, 1, 0, "%s", error_local->message);
      g_error_free (error_local);
      return FALSE;
    }

  if (!g_strcmp0 (action, "Suspend"))
    {
      gint seconds = 0;
      ret = dbus_g_proxy_call (proxy, action, &error_local,
			       G_TYPE_INT, seconds,
			       G_TYPE_INVALID,
			       G_TYPE_INT, &return_code,
			       G_TYPE_INVALID);
    }
  else
    {
      ret = dbus_g_proxy_call (proxy, action, &error_local,
			       G_TYPE_INVALID,
			       G_TYPE_INT, &return_code,
			       G_TYPE_INVALID);
    }

  g_object_unref (proxy);

  if ( !ret )
    {
      /* A D-Bus timeout? */
      if ( g_error_matches (error_local, DBUS_GERROR, DBUS_GERROR_NO_REPLY) )
	{
	  g_error_free (error_local);
	  return TRUE;
	}
      else
	{
	  g_set_error (error, 1, 0, "%s", error_local->message);
	  g_error_free (error_local);
	  return FALSE;
	}
    }
  
  return TRUE;
}
#endif /*ENABLE_HAL*/

/**
 * xfsm_shutdown_helper_sudo_send:
 *
 *
 **/
static gboolean
xfsm_shutdown_helper_sudo_send (XfsmShutdownHelper *helper,
				const gchar *action,
				GError **error)
{
  gchar  response[256];
  
  fprintf (helper->outfile, "%s\n", action);
  fflush (helper->outfile);

  g_message (G_STRLOC ": Using ConsoleKit to %s",action);
  
  if (ferror (helper->outfile))
    {
      if (errno == EINTR)
	{
	  /* probably succeeded but the helper got killed */
	  return TRUE;
	}
      
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
		   _("Error sending command to shutdown helper: %s"),
		   strerror (errno));
      return FALSE;
    }

  if (fgets (response, 256, helper->infile) == NULL)
    {
      if (errno == EINTR)
	{
	  /* probably succeeded but the helper got killed */
	  return TRUE;
	}
      
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
		   _("Error receiving response from shutdown helper: %s"),
		   strerror (errno));
      
      return FALSE;
    }

  if (strncmp (response, "SUCCEED", 7) != 0)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
		   _("Shutdown command failed"));
      
      return FALSE;
    
    }
  return TRUE;
}

/**
 * xfsm_shutdown_helper_new:
 *
 * 
 **/
XfsmShutdownHelper *
xfsm_shutdown_helper_new (void)
{
  XfsmShutdownHelper *xfsm_shutdown_helper = NULL;

  xfsm_shutdown_helper = g_object_new (XFSM_TYPE_SHUTDOWN_HELPER, NULL);
  
  return xfsm_shutdown_helper;
}

/**
 * xfsm_shutdown_helper_send_password:
 *
 * Send password to sudo
 *
 **/
gboolean xfsm_shutdown_helper_send_password (XfsmShutdownHelper *helper, const gchar *password)
{
  gssize result;
  gchar  buffer[1024];
  gsize  failed;
  gsize  length;
  gsize  bytes;
  gint   fd;

  g_return_val_if_fail (password != NULL, FALSE);
  g_return_val_if_fail (helper->shutdown_backend == XFSM_SHUTDOWN_BACKEND_SUDO, FALSE);
  g_return_val_if_fail (helper->require_password, FALSE);

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
              helper->require_password = FALSE;
              break;
            }
        } 
    }

  return TRUE;
}

/**
 * xfsm_shutdown_helper_shutdown:
 *
 *
 **/
gboolean xfsm_shutdown_helper_shutdown (XfsmShutdownHelper *helper, GError **error)
{
  g_return_val_if_fail (!error || !*error, FALSE);
  
#ifdef ENABLE_CONSOLE_KIT
  if ( helper->shutdown_backend == XFSM_SHUTDOWN_BACKEND_CONSOLE_KIT )
    {
      return xfsm_shutdown_helper_console_kit_shutdown (helper, "Stop", error);
    }
#endif

#ifdef ENABLE_HAL
  if ( helper->shutdown_backend == XFSM_SHUTDOWN_BACKEND_HAL )
    {
      return xfsm_shutdown_helper_hal_send (helper, "Shutdown", error);
    }
#endif
  
  /*Use Sudo mode*/
  return xfsm_shutdown_helper_sudo_send (helper, "POWEROFF", error);

}

/**
 * xfsm_shutdown_helper_restart:
 *
 *
 **/
gboolean xfsm_shutdown_helper_restart (XfsmShutdownHelper *helper, GError **error)
{
  g_return_val_if_fail (!error || !*error, FALSE);

#ifdef ENABLE_CONSOLE_KIT
  if ( helper->shutdown_backend == XFSM_SHUTDOWN_BACKEND_CONSOLE_KIT )
    {
      return xfsm_shutdown_helper_console_kit_shutdown (helper, "Restart", error);
    }
#endif

#ifdef ENABLE_HAL
  if ( helper->shutdown_backend == XFSM_SHUTDOWN_BACKEND_HAL )
    {
      return xfsm_shutdown_helper_hal_send (helper, "Reboot", error);
    }
#endif

  return xfsm_shutdown_helper_sudo_send (helper, "REBOOT", error);

}

/**
 * xfsm_shutdown_helper_suspend:
 *
 **/
gboolean xfsm_shutdown_helper_suspend (XfsmShutdownHelper *helper, GError **error)
{
  g_return_val_if_fail (!error || !*error, FALSE);

#ifdef ENABLE_UPOWER
  if ( helper->sleep_backend == XFSM_SLEEP_BACKEND_UPOWER )
    {
      return xfsm_shutdown_helper_upower_sleep (helper, "Suspend", error);
    }
#endif

#ifdef ENABLE_HAL
  if ( helper->sleep_backend == XFSM_SLEEP_BACKEND_HAL )
    {
      return xfsm_shutdown_helper_hal_send (helper, "Suspend", error);
    }
#endif

  g_set_error (error, 1, 0, "%s", _("Suspend failed, no backend supported"));

  return FALSE;
}

/**
 * xfsm_shutdown_helper_hibernate:
 *
 **/
gboolean xfsm_shutdown_helper_hibernate (XfsmShutdownHelper *helper, GError **error)
{
  g_return_val_if_fail (!error || !*error, FALSE);

#ifdef ENABLE_UPOWER
  if ( helper->sleep_backend == XFSM_SLEEP_BACKEND_UPOWER )
    {
      return xfsm_shutdown_helper_upower_sleep (helper, "Hibernate", error);
    }
#endif

#ifdef ENABLE_HAL
  if ( helper->sleep_backend == XFSM_SLEEP_BACKEND_HAL )
    {
      return xfsm_shutdown_helper_hal_send (helper, "Hibernate", error);
    }
#endif

  g_set_error (error, 1, 0, "%s", _("Hibernate failed, no backend supported"));

  return FALSE;
}

/**
 * xfsm_shutdown_helper_send_command:
 *
 **/
gboolean xfsm_shutdown_helper_send_command (XfsmShutdownHelper *helper,
					    XfsmShutdownType shutdown_type,
					    GError **error)
{
  g_return_val_if_fail (!error || !*error, FALSE);
  
  switch (shutdown_type)
    {
    case XFSM_SHUTDOWN_HALT:
      return xfsm_shutdown_helper_shutdown (helper, error);
    case XFSM_SHUTDOWN_REBOOT:
      return xfsm_shutdown_helper_restart (helper, error);
    case XFSM_SHUTDOWN_HIBERNATE:
      return xfsm_shutdown_helper_hibernate (helper, error);
    case XFSM_SHUTDOWN_SUSPEND:
      return xfsm_shutdown_helper_suspend (helper, error);
    default:
      g_warn_if_reached ();
      break;
    }

  g_set_error (error, 1, 0, "%s", _("Shutdown Command not found"));

  return FALSE;
}
