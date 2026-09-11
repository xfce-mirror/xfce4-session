/*
 * Copyright (c) 2023 Gaël Bonithon <gael@xfce.org>
 * Copyright (c) 2009 Brian Tarricone <brian@terricone.org>
 * Copyright (C) 1999 Olivier Fourdan <fourdan@xfce.org>
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

#if !defined(_LIBXFCE4SESSION_CLIENT_INSIDE_LIBXFCE4SESSION_CLIENT_H) && !defined(LIBXFCE4SESSION_CLIENT_COMPILATION)
#error "Only <libxfce4session-client/libxfce4session-client.h> can be included directly, this file is not part of the public API."
#endif

#ifndef __LIBXFCE4SESSION_CLIENT_ENUMS_H__
#define __LIBXFCE4SESSION_CLIENT_ENUMS_H__

#include <glib.h>

G_BEGIN_DECLS

/**
 * XfceSessionClientErrorEnum:
 * @XFCE_SESSION_CLIENT_ERROR_FAILED: Failed to connect to the session manager.
 * @XFCE_SESSION_CLIENT_ERROR_INVALID_CLIENT: Session does not have a valid client id.
 *
 * Error codes returned by XfceSessionClient functions.
 **/
typedef enum
{
  XFCE_SESSION_CLIENT_ERROR_FAILED,
  XFCE_SESSION_CLIENT_ERROR_INVALID_CLIENT
} XfceSessionClientErrorEnum;

typedef enum
{
  XFCE_SESSION_CLIENT_RESTART_NORMAL = 0,
  XFCE_SESSION_CLIENT_RESTART_IMMEDIATELY,
} XfceSessionClientRestartStyle;

typedef enum /*< skip >*/
{
  XFCE_SESSION_CLIENT_PRIORITY_HIGHEST = 0,
  XFCE_SESSION_CLIENT_PRIORITY_WM = 15,
  XFCE_SESSION_CLIENT_PRIORITY_CORE = 25,
  XFCE_SESSION_CLIENT_PRIORITY_DESKTOP = 35,
  XFCE_SESSION_CLIENT_PRIORITY_DEFAULT = 50,
  XFCE_SESSION_CLIENT_PRIORITY_LOWEST = 255,
} XfceSessionClientPriority;

typedef enum
{
  XFCE_SESSION_CLIENT_SHUTDOWN_HINT_ASK = 0,
  XFCE_SESSION_CLIENT_SHUTDOWN_HINT_LOGOUT,
  XFCE_SESSION_CLIENT_SHUTDOWN_HINT_HALT,
  XFCE_SESSION_CLIENT_SHUTDOWN_HINT_REBOOT,
} XfceSessionClientShutdownHint;

G_END_DECLS

#endif /* __LIBXFCE4SESSION_CLIENT_ENUMS_H__ */
