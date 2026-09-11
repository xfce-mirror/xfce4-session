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

#ifndef __XFCE_SESSION_CLIENT_DBUS_H__
#define __XFCE_SESSION_CLIENT_DBUS_H__

#include "xfce-session-client-private.h"
#include "xfsm-manager-dbus-client.h"

G_BEGIN_DECLS

G_DECLARE_DERIVABLE_TYPE (XfceSessionClientDBus, xfce_session_client_dbus, XFCE, SESSION_CLIENT_DBUS, XfceSessionClient)
#define XFCE_TYPE_SESSION_CLIENT_DBUS (xfce_session_client_dbus_get_type ())

typedef gboolean (*XfceSessionClientDBusRegisterFunc) (XfceSessionClientDBus *dsession_client,
                                                       GError **error);

struct _XfceSessionClientDBusClass
{
  XfceSessionClientClass parent_class;
};

gboolean
_xfce_session_client_dbus_do_connect (XfceSessionClientDBus *dsession_client,
                                      XfceSessionClientDBusRegisterFunc register_func,
                                      GError **error);

XfsmDbusManager *
_xfce_session_client_dbus_get_xfsm_manager_proxy (XfceSessionClientDBus *dsession_client);
void
_xfce_session_client_dbus_set_client_object_path (XfceSessionClientDBus *dsession_client,
                                                  const gchar *object_path);

G_END_DECLS

#endif /* !__XFCE_SESSION_CLIENT_DBUS_H__ */
