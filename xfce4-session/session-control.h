/*
 * Copyright (c) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
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

#ifndef __XFSM_SESSION_CONTROL_H__
#define __XFSM_SESSION_CONTROL_H__

#include <gtk/gtk.h>

#include <xfce4-session/client.h>

G_BEGIN_DECLS

#define	XFSM_SESSION_CONTROL(obj)					\
	G_TYPE_CHECK_INSTANCE_CAST(obj, xfsm_session_control_get_type(),\
			XfsmSessionControl)
#define XFSM_SESSION_CONTROL_CLASS(klass)				\
	G_TYPE_CHECK_CLASS_CAST(klass, xfsm_session_control_get_type(),	\
			XfsmSessionControlClass)
#define XFSM_IS_SESSION_CONTROL(obj)					\
	G_TYPE_CHECK_INSTANCE_TYPE(obj, xfsm_session_control_get_type())

typedef struct	_XfsmSessionControl		XfsmSessionControl;
typedef struct	_XfsmSessionControlClass	XfsmSessionControlClass;

struct _XfsmSessionControlClass
{
	GtkDialogClass	parent_class;
};

struct _XfsmSessionControl
{
	GtkDialog	dialog;

	/* */
	GList		*clients;

	/* */
	GtkWidget	*tree;

	/* buttons */
	GtkWidget	*cloneButton;
	GtkWidget	*closeButton;
	GtkWidget	*killButton;
};

/* prototypes */
extern GType		xfsm_session_control_get_type(void);
extern GtkWidget	*xfsm_session_control_new(void);
extern void		xfsm_session_control_append(XfsmSessionControl *,
				Client *);
extern void		xfsm_session_control_update(XfsmSessionControl *,
				Client *);
extern void		xfsm_session_control_remove(XfsmSessionControl *,
				Client *);

G_END_DECLS

#endif	/* !__XFSM_SESSION_CONTROL_H__ */
