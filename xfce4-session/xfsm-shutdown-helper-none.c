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


#include <xfce4-session/xfsm-shutdown-helper.h>


struct _XfmShutdownHelper
{
  gint dummy;
};


XfsmShutdownHelper*
xfsm_shutdown_helper_spawn (void)
{
  return NULL;
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


gboolean
xfsm_shutdown_helper_send_command (XfsmShutdownHelper  *helper,
                                   XfsmShutdownCommand  command)
{
  return FALSE;
}


void
xfsm_shutdown_helper_destroy (XfsmShutdownHelper *helper)
{
}


