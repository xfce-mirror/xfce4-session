/*-
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

#ifndef __XFSM_UPOWER_HELPER_H__
#define __XFSM_UPOWER_HELPER_H__

typedef struct _XfsmUPowerClass XfsmUPowerClass;
typedef struct _XfsmUPower      XfsmUPower;

#define XFSM_TYPE_UPOWER            (xfsm_upower_get_type ())
#define XFSM_UPOWER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFSM_TYPE_UPOWER, XfsmUPower))
#define XFSM_UPOWER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFSM_TYPE_UPOWER, XfsmUPowerClass))
#define XFSM_IS_UPOWER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFSM_TYPE_UPOWER))
#define XFSM_IS_UPOWER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFSM_TYPE_UPOWER))
#define XFSM_UPOWER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFSM_TYPE_UPOWER, XfsmUPowerClass))

GType           xfsm_upower_get_type         (void) G_GNUC_CONST;

XfsmUPower     *xfsm_upower_get              (void);

gboolean        xfsm_upower_try_suspend      (XfsmUPower      *upower,
                                              GError         **error);

gboolean        xfsm_upower_try_hibernate    (XfsmUPower      *upower,
                                              GError         **error);

gboolean        xfsm_upower_can_suspend      (XfsmUPower      *upower,
                                              gboolean        *can_suspend,
                                              gboolean        *auth_suspend,
                                              GError         **error);

gboolean        xfsm_upower_can_hibernate    (XfsmUPower      *upower,
                                              gboolean        *can_hibernate,
                                              gboolean        *auth_hibernate,
                                              GError         **error);

gboolean        xfsm_upower_lock_screen      (XfsmUPower      *upower,
                                              const gchar     *sleep_kind,
                                              GError         **error);

#endif /* !__XFSM_UPOWER_HELPER_H__ */
