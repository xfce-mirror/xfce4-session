/* $Id$ */
/*-
 * Copyright (c) 2006 Jani Monoses <jani@ubuntu.com> 
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <dbus/dbus.h>

#include <xfce4-session/xfsm-shutdown-helper.h>



#define HAL_DBUS_SERVICE          "org.freedesktop.Hal"
#define HAL_ROOT_COMPUTER         "/org/freedesktop/Hal/devices/computer"
#define HAL_DBUS_INTERFACE_POWER  "org.freedesktop.Hal.Device.SystemPowerManagement"



struct _XfsmShutdownHelper
{
  gint dummy;
};


XfsmShutdownHelper*
xfsm_shutdown_helper_spawn (void)
{
  return g_new0 (XfsmShutdownHelper, 1);
}


gboolean
xfsm_shutdown_helper_need_password (const XfsmShutdownHelper *helper)
{
  return FALSE;
}


gboolean
xfsm_shutdown_helper_send_password (XfsmShutdownHelper *helper,
                                    const gchar        *password)
{
  return TRUE;
}


static gboolean 
hal_dbus_power_command(XfsmShutdownCommand command)
{
  DBusConnection *connection;
  DBusMessage    *method;
  DBusMessage    *result;
  const gchar    *methodname;
  DBusError       derror;
  
  if (command == XFSM_SHUTDOWN_REBOOT)
    methodname = "Reboot";
  else
    methodname = "Shutdown";
 
  dbus_error_init (&derror);
  connection = dbus_bus_get (DBUS_BUS_SYSTEM, &derror);
  if (connection == NULL)
    {
      dbus_error_free (&derror);
      return FALSE;
    }
  
  method = dbus_message_new_method_call (HAL_DBUS_SERVICE,
                                         HAL_ROOT_COMPUTER,
                                         HAL_DBUS_INTERFACE_POWER,
                                         methodname);
  result = dbus_connection_send_with_reply_and_block (connection, method, 2000, &derror);
  dbus_message_unref (method);
  
  if (result == NULL)
    {
      dbus_error_free (&derror);
      return FALSE;
    }
  
  dbus_message_unref (result);
  return TRUE;
}


gboolean
xfsm_shutdown_helper_send_command (XfsmShutdownHelper *helper,
                                   XfsmShutdownCommand command)
{
  return hal_dbus_power_command (command); 
}


void
xfsm_shutdown_helper_destroy (XfsmShutdownHelper *helper)
{
  g_free (helper);
}


