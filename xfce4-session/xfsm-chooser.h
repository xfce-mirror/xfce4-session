/* $Id$ */
/*-
 * Copyright (c) 2004 Benedikt Meurer <benny@xfce.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __XFSM_CHOOSER_H__
#define __XFSM_CHOOSER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XFSM_TYPE_CHOOSER xfsm_chooser_get_type()
#define XFSM_CHOOSER(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, XFSM_TYPE_CHOOSER, XfsmChooser)
#define XFSM_CHOOSER_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, XFSM_TYPE_CHOOSER, XfsmChooserClass)
#define XFSM_IS_CHOOSER(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, XFSM_TYPE_CHOOSER)

typedef struct _XfsmChooser XfsmChooser;
typedef struct _XfsmChooserClass XfsmChooserClass;
typedef struct _XfsmChooserSession XfsmChooserSession;

struct _XfsmChooserClass
{
  GtkDialogClass parent_class;
};

struct _XfsmChooser
{
  GtkDialog dialog;

  GtkWidget *radio_load;
  GtkWidget *radio_create;
  GtkWidget *tree;
  GtkWidget *name;
  GtkWidget *start;

  GtkTooltips *tooltips;
};

struct _XfsmChooserSession
{
  gchar *name;
  time_t atime;
};

typedef enum _XfsmChooserReturn
{
  XFSM_CHOOSER_CREATE,
  XFSM_CHOOSER_LOAD,
  XFSM_CHOOSER_LOGOUT,
} XfsmChooserReturn;

GType xfsm_chooser_get_type (void) G_GNUC_CONST;
void xfsm_chooser_set_sessions (XfsmChooser *chooser, GList *sessions,
                               const gchar *default_session);
XfsmChooserReturn xfsm_chooser_run (XfsmChooser *chooser, gchar **name);

G_END_DECLS;

#endif /* !__XFSM_CHOOSER_H__ */
