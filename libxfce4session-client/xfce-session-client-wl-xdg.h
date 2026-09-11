/*
 * Copyright (c) 2026 Brian Tarricone <brian@terricone.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef __XFCE_SESSION_CLIENT_WL_XDG_H__
#define __XFCE_SESSION_CLIENT_WL_XDG_H__

#include "xfce-session-client-dbus.h"

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (XfceSessionClientWlXdg, xfce_session_client_wl_xdg, XFCE, SESSION_CLIENT_WL_XDG, XfceSessionClientDBus)
#define XFCE_TYPE_SESSION_CLIENT_WL_XDG (xfce_session_client_wl_xdg_get_type ())

G_END_DECLS

#endif /* !__XFCE_SESSION_CLIENT_WL_XDG_H__ */
