/* $Id$ */
/*-
 * Copyright (c) 2004 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
