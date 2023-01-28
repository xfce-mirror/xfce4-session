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

#ifndef __XFSM_INHIBITOR_H
#define __XFSM_INHIBITOR_H

#include <glib-object.h>

#include "xfsm-inhibition.h"

G_BEGIN_DECLS

#define XFSM_TYPE_INHIBITOR         (xfsm_inhibitor_get_type ())
#define XFSM_INHIBITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), XFSM_TYPE_INHIBITOR, XfsmInhibitor))
#define XFSM_INHIBITOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), XFSM_TYPE_INHIBITOR, XfsmInhibitorClass))
#define XFSM_IS_INHIBITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFSM_TYPE_INHIBITOR))
#define XFSM_IS_INHIBITOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), XFSM_TYPE_INHIBITOR))
#define XFSM_INHIBITOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), XFSM_TYPE_INHIBITOR, XfsmInhibitorClass))

typedef struct XfsmInhibitorPrivate XfsmInhibitorPrivate;

typedef struct
{
  GObject                      parent;
  XfsmInhibitorPrivate *priv;
} XfsmInhibitor;

typedef struct
{
  GObjectClass   parent_class;
} XfsmInhibitorClass;

GType           xfsm_inhibitor_get_type                 (void);

gboolean        xfsm_inhibitor_add                      (XfsmInhibitor  *store,
                                                         XfsmInhibition *inhibition);

gboolean        xfsm_inhibitor_remove                   (XfsmInhibitor  *store,
                                                         guint           cookie);

gboolean        xfsm_inhibitor_is_empty                 (XfsmInhibitor  *store);

gboolean        xfsm_inhibitor_has_flags                (XfsmInhibitor  *store,
                                                         guint           flags);

XfsmInhibitor * xfsm_inhibitor_get                      (void);

G_END_DECLS

#endif /* __XFSM_INHIBITOR_H */
