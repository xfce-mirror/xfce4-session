/*
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA.
 */

#include "xfsm-error.h"

#define XFSM_DBUS_NAME "org.xfce.SessionManager"

static const GDBusErrorEntry xfsm_error_entries[] = {
  { XFSM_ERROR_BAD_STATE, XFSM_DBUS_NAME ".Error.Failed" },
  { XFSM_ERROR_BAD_VALUE, XFSM_DBUS_NAME ".Error.General" },
  { XFSM_ERROR_UNSUPPORTED, XFSM_DBUS_NAME ".Error.Unsupported" },
};

GQuark
xfsm_error_get_quark (void)
{
  static volatile gsize quark_volatile = 0;

  g_dbus_error_register_error_domain ("xfsm_error",
                                      &quark_volatile,
                                      xfsm_error_entries,
                                      G_N_ELEMENTS (xfsm_error_entries));

  return (GQuark) quark_volatile;
}

GType
xfsm_error_get_type (void)
{
  static GType xfsm_error_type = 0;

  if (G_UNLIKELY (xfsm_error_type == 0))
    {
      static const GEnumValue values[] = {
        { XFSM_ERROR_BAD_STATE, "XFSM_ERROR_BAD_STATE", "BadState" },
        { XFSM_ERROR_BAD_VALUE, "XFSM_ERROR_BAD_VALUE", "BadValue" },
        { XFSM_ERROR_UNSUPPORTED, "XFSM_ERROR_UNSUPPORTED", "Unsupported" },
        { 0, NULL, NULL },
      };

      xfsm_error_type = g_enum_register_static ("XfsmError", values);
    }

  return xfsm_error_type;
}

void
throw_error (GDBusMethodInvocation *context,
             gint error_code,
             const gchar *format,
             ...)
{
  va_list args;
  gchar *message;

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  g_dbus_method_invocation_return_error (context, XFSM_ERROR, error_code, "%s", message);

  g_free (message);
}
