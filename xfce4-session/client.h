/* $Id$ */
/*-
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

#ifndef __XFSM_CLIENT_H__
#define __XFSM_CLIENT_H__

#include <stdio.h>

#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>

#include <glib.h>

/* client states */
enum
{
	CLIENT_IDLE = 0,
	CLIENT_INTERACTING,
	CLIENT_SAVEDONE,
	CLIENT_SAVING,
	CLIENT_SAVINGLOCAL,
	CLIENT_WAITFORINTERACT,
	CLIENT_WAITFORPHASE2,
	CLIENT_DISCONNECTED,
};

typedef struct _Client	Client;
struct _Client
{
	/* identification */
	char		*id;

	/* clients host machine */
	char		*hostname;

	/* connection handles */
	IceConn		iceConn;
	SmsConn		smsConn;

	/* session properties */
	int		nProps;
	SmProp		**props;

	/* client state */
	int		state;
};

#define CLIENT(obj)		((Client *)(obj))

/* prototypes */
extern Client	*client_new(SmsConn);
extern void	client_free(Client *);
extern gboolean	client_save(const Client *, FILE *fp);
extern Client	*client_load(FILE *fp);
extern gchar	*client_get_current_directory(const Client *);
extern gchar	*client_get_program(const Client *);
extern gchar	*client_get_userid(const Client *);
extern gchar	**client_get_restart_command(const Client *);
extern gint	client_get_priority(const Client *);
extern gint	client_get_restart_style_hint(const Client *);
extern gint	client_compare_priority(const Client *, const Client *);
extern gchar 	**client_get_command(const Client *, const gchar *);
extern gboolean	client_run_command(const Client *, const gchar *, GError **);

extern void	client_kill(const Client *);

#define	client_get_id(client)		(g_strdup(CLIENT((client))->id))
#define	client_get_hostname(client)	(g_strdup(CLIENT((client))->hostname))

#define client_state(client)		(CLIENT((client))->state)

#endif	/* !__XFSM_CLIENT_H__ */
