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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <dbus/dbus-glib.h>

#include <xfce4-session/xfsm-error.h>


GQuark
xfsm_error_get_quark (void)
{
  static GQuark xfsm_error_quark = 0;

  if (G_UNLIKELY (xfsm_error_quark == 0))
    xfsm_error_quark = g_quark_from_static_string ("xfsm-error-quark");

  return xfsm_error_quark;
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
xfsm_error_dbus_init (void)
{
  dbus_g_error_domain_register (XFSM_ERROR, "org.xfce.Session.Manager",
                                XFSM_TYPE_ERROR);
}
