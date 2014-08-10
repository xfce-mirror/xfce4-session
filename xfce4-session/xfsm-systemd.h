/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Christian Hesse
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

#ifndef __XFSM_SYSTEMD_H__
#define __XFSM_SYSTEMD_H__

#define LOGIND_RUNNING() (access ("/run/systemd/seats/", F_OK) >= 0)

typedef struct _XfsmSystemdClass XfsmSystemdClass;
typedef struct _XfsmSystemd      XfsmSystemd;

#define XFSM_TYPE_SYSTEMD            (xfsm_systemd_get_type ())
#define XFSM_SYSTEMD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFSM_TYPE_SYSTEMD, XfsmSystemd))
#define XFSM_SYSTEMD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFSM_TYPE_SYSTEMD, XfsmSystemdClass))
#define XFSM_IS_SYSTEMD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFSM_TYPE_SYSTEMD))
#define XFSM_IS_SYSTEMD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFSM_TYPE_SYSTEMD))
#define XFSM_SYSTEMD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFSM_TYPE_SYSTEMD, XfsmSystemdClass))

GType           xfsm_systemd_get_type     (void) G_GNUC_CONST;

XfsmSystemd *xfsm_systemd_get          (void);

gboolean     xfsm_systemd_try_restart  (XfsmSystemd  *systemd,
                                        GError      **error);

gboolean     xfsm_systemd_try_shutdown (XfsmSystemd  *systemd,
                                        GError      **error);

gboolean     xfsm_systemd_try_suspend  (XfsmSystemd  *systemd,
                                        GError      **error);

gboolean     xfsm_systemd_try_hibernate (XfsmSystemd *systemd,
                                        GError      **error);

gboolean     xfsm_systemd_can_restart  (XfsmSystemd  *systemd,
                                        gboolean     *can_restart,
                                        GError      **error);

gboolean     xfsm_systemd_can_shutdown (XfsmSystemd  *systemd,
                                        gboolean     *can_shutdown,
                                        GError      **error);

gboolean     xfsm_systemd_can_suspend  (XfsmSystemd  *systemd,
                                        gboolean     *can_suspend,
                                        gboolean     *auth_suspend,
                                        GError      **error);

gboolean     xfsm_systemd_can_hibernate (XfsmSystemd *systemd,
                                        gboolean     *can_hibernate,
                                        gboolean     *auth_hibernate,
                                        GError      **error);

G_END_DECLS

#endif  /* __XFSM_SYSTEMD_H__ */
