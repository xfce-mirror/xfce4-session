/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2010      Ali Abdallah    <aliov@xfce.org>
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
 */

#ifndef __XFSM_CONSOLEKIT_HELPER_H__
#define __XFSM_CONSOLEKIT_HELPER_H__

typedef struct _XfsmConsolekitClass XfsmConsolekitClass;
typedef struct _XfsmConsolekit      XfsmConsolekit;

#define XFSM_TYPE_CONSOLEKIT            (xfsm_consolekit_get_type ())
#define XFSM_CONSOLEKIT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFSM_TYPE_CONSOLEKIT, XfsmConsolekit))
#define XFSM_CONSOLEKIT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFSM_TYPE_CONSOLEKIT, XfsmConsolekitClass))
#define XFSM_IS_CONSOLEKIT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFSM_TYPE_CONSOLEKIT))
#define XFSM_IS_CONSOLEKIT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFSM_TYPE_CONSOLEKIT))
#define XFSM_CONSOLEKIT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFSM_TYPE_CONSOLEKIT, XfsmConsolekitClass))

GType           xfsm_consolekit_get_type     (void) G_GNUC_CONST;

XfsmConsolekit *xfsm_consolekit_get          (void);

gboolean        xfsm_consolekit_try_restart  (XfsmConsolekit  *consolekit,
                                              GError         **error);

gboolean        xfsm_consolekit_try_shutdown (XfsmConsolekit  *consolekit,
                                              GError         **error);

gboolean        xfsm_consolekit_can_restart  (XfsmConsolekit  *consolekit,
                                              gboolean        *can_restart,
                                              GError         **error);

gboolean        xfsm_consolekit_can_shutdown (XfsmConsolekit  *consolekit,
                                              gboolean        *can_shutdown,
                                              GError         **error);

gboolean        xfsm_consolekit_try_suspend  (XfsmConsolekit  *consolekit,
                                              GError         **error);

gboolean        xfsm_consolekit_try_hibernate(XfsmConsolekit  *consolekit,
                                              GError         **error);

gboolean        xfsm_consolekit_can_suspend  (XfsmConsolekit  *consolekit,
                                              gboolean        *can_suspend,
                                              gboolean        *auth_suspend,
                                              GError         **error);

gboolean        xfsm_consolekit_can_hibernate(XfsmConsolekit  *consolekit,
                                              gboolean        *can_hibernate,
                                              gboolean        *auth_hibernate,
                                              GError         **error);


#endif /* !__XFSM_CONSOLEKIT_HELPER_H__ */
