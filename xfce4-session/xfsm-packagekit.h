/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2021 Matias De lellis
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __XFSM_PACKAGEKIT_H__
#define __XFSM_PACKAGEKIT_H__

typedef struct _XfsmPackagekitClass XfsmPackagekitClass;
typedef struct _XfsmPackagekit      XfsmPackagekit;

#define XFSM_TYPE_PACKAGEKIT            (xfsm_packagekit_get_type ())
#define XFSM_PACKAGEKIT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFSM_TYPE_PACKAGEKIT, XfsmPackagekit))
#define XFSM_PACKAGEKIT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFSM_TYPE_PACKAGEKIT, XfsmPackagekitClass))
#define XFSM_IS_PACKAGEKIT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFSM_TYPE_PACKAGEKIT))
#define XFSM_IS_PACKAGEKIT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFSM_TYPE_PACKAGEKIT))
#define XFSM_PACKAGEKIT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFSM_TYPE_PACKAGEKIT, XfsmPackagekitClass))

GType           xfsm_packagekit_get_type             (void) G_GNUC_CONST;


XfsmPackagekit *xfsm_packagekit_get                  (void);

gboolean        xfsm_packagekit_has_update_prepared  (XfsmPackagekit  *packagekit,
                                                      gboolean        *update_prepared,
                                                      GError         **error);

gboolean        xfsm_packagekit_try_trigger_shutdown (XfsmPackagekit  *packagekit,
                                                      GError         **error);

gboolean        xfsm_packagekit_try_trigger_restart  (XfsmPackagekit  *packagekit,
                                                      GError         **error);


G_END_DECLS

#endif  /* __XFSM_PACKAGEKIT_H__ */
