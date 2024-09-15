/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2021 Matias De lellis <mati86dl@gmail.com>
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

#ifndef __XFSM_INHIBITON_H__
#define __XFSM_INHIBITON_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define XFSM_TYPE_INHIBITION (xfsm_inhibition_get_type ())
G_DECLARE_FINAL_TYPE (XfsmInhibition, xfsm_inhibition, XFSM, INHIBITION, GObject)

/**
 * XfsmInhibitonFlag:
 * @XFSM_INHIBITON_FLAG_LOGOUT: Inhibit ending the user session by logging out or by shutting down the computer
 * @XFSM_INHIBITON_FLAG_SWITCH: Inhibit user switching
 * @XFSM_INHIBITON_FLAG_SUSPEND: Inhibit suspending the session or computer
 * @XFSM_INHIBITON_FLAG_IDLE: Inhibit the session being marked as idle (and possibly locked)
 */
typedef enum
{
  XFSM_INHIBITON_FLAG_LOGOUT = 1 << 0,
  XFSM_INHIBITON_FLAG_SWITCH = 1 << 1,
  XFSM_INHIBITON_FLAG_SUSPEND = 1 << 2,
  XFSM_INHIBITON_FLAG_IDLE = 1 << 3
} XfsmInhibitonFlag;

XfsmInhibition *
xfsm_inhibition_new (const char *app_id,
                     guint toplevel_xid,
                     XfsmInhibitonFlag flags,
                     const char *reason);

const char *
xfsm_inhibition_get_app_id (XfsmInhibition *inhibition);
const char *
xfsm_inhibition_get_reason (XfsmInhibition *inhibition);
guint
xfsm_inhibition_get_cookie (XfsmInhibition *inhibition);
XfsmInhibitonFlag
xfsm_inhibition_get_flags (XfsmInhibition *inhibition);
guint
xfsm_inhibition_get_toplevel_xid (XfsmInhibition *inhibition);
guint *
xfsm_inhibition_peek_cookie (XfsmInhibition *inhibition);

G_END_DECLS

#endif /* __XFSM_INHIBITON_H__ */
