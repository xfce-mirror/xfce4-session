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

#ifndef __XFSM_CLIENT_LIST_H__
#define __XFSM_CLIENT_LIST_H__

#include <gtk/gtk.h>

#include <xfce4-session/client.h>

G_BEGIN_DECLS

#define	XFSM_CLIENT_LIST(obj)						\
	G_TYPE_CHECK_INSTANCE_CAST(obj, xfsm_client_list_get_type(),	\
			XfsmClientList)
#define XFSM_CLIENT_LIST_CLASS(klass)					\
	G_TYPE_CHECK_CLASS_CAST(klass, xfsm_client_list_get_type(),	\
			XfsmClientListClass)
#define XFSM_IS_CLIENT_LIST(obj)					\
	G_TYPE_CHECK_INSTANCE_TYPE(obj, xfsm_client_list_get_type())

typedef struct	_XfsmClientList		XfsmClientList;
typedef struct	_XfsmClientListClass	XfsmClientListClass;

struct _XfsmClientListClass
{
	GtkDialogClass	parent_class;
};

struct _XfsmClientList
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
extern GType		xfsm_client_list_get_type(void);
extern GtkWidget	*xfsm_client_list_new(void);
extern void		xfsm_client_list_append(XfsmClientList *, Client *);
extern void		xfsm_client_list_update(XfsmClientList *, Client *);
extern void		xfsm_client_list_remove(XfsmClientList *, Client *);

G_END_DECLS

#endif	/* !__XFSM_CLIENT_LIST_H__ */
