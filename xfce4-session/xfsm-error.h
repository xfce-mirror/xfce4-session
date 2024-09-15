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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __XFSM_ERRORS_H__
#define __XFSM_ERRORS_H__

#include <gio/gio.h>
#include <glib-object.h>

#define XFSM_TYPE_ERROR (xfsm_error_get_type ())
#define XFSM_ERROR (xfsm_error_get_quark ())

#define ERROR_MSG(err) ((err) != NULL ? (err)->message : "Error not set")

G_BEGIN_DECLS

typedef enum
{
  XFSM_ERROR_BAD_STATE = 0,
  XFSM_ERROR_BAD_VALUE,
  XFSM_ERROR_UNSUPPORTED,
} XfsmError;

GType
xfsm_error_get_type (void) G_GNUC_CONST;
GQuark
xfsm_error_get_quark (void) G_GNUC_CONST;

void
throw_error (GDBusMethodInvocation *context,
             gint error_code,
             const gchar *format,
             ...);

G_END_DECLS

#endif /* !__XFSM_ERRORS_H__ */
