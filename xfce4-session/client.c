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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>

#include <glib.h>

#include "client.h"
#include "util.h"

/*
 */
Client *
client_new(SmsConn smsConn)
{
	Client *client;

	g_return_val_if_fail(smsConn != NULL, NULL);

	client = g_new0(Client, 1);
	client->iceConn = SmsGetIceConnection(smsConn);
	client->smsConn = smsConn;

	return(client);
}

/*
 */
void
client_free(Client *client)
{
	g_return_if_fail(client != NULL);

	if (client->props != NULL) {
		while (client->nProps-- > 0)
			SmFreeProperty(client->props[client->nProps]);
		free(client->props);
	}

	if (client->hostname != NULL)
		free(client->hostname);

	if (client->id != NULL)
		free(client->id);

	g_free(client);
}

/*
 */
gboolean
client_save(const Client *client, FILE *fp)
{
	SmPropValue *ve, *vp;
	SmProp **pp, **pe;

	g_return_val_if_fail(client != NULL, FALSE);
	g_return_val_if_fail(client->id != NULL, FALSE);
	g_return_val_if_fail(client->hostname != NULL, FALSE);

	pe = client->props + client->nProps;

	/* save client information */
	fstrwrite(fp, client->id);
	fstrwrite(fp, client->hostname);

	/* store properties */
	fwrite(&client->nProps, sizeof(client->nProps), 1, fp);

	for (pp = client->props; pp < pe; pp++) {
		ve = (*pp)->vals + (*pp)->num_vals;

		fstrwrite(fp, (*pp)->name);
		fstrwrite(fp, (*pp)->type);

		/* store values */
		fwrite(&((*pp)->num_vals), sizeof((*pp)->num_vals), 1, fp);

		for (vp = (*pp)->vals; vp < ve; vp++) {
			fwrite(&(vp->length), sizeof(vp->length), 1, fp);
			fwrite(vp->value, 1, vp->length, fp);
		}
	}

	return(!ferror(fp));
}

/*
 */
Client *
client_load(FILE *fp)
{
	SmPropValue *values, *ve, *vp;
	SmProp **props, **pp, **pe;
	Client *client;
	int n, m, l;

	client = g_new0(Client, 1);

	if ((client->id = fstrread(fp)) == NULL ||
	    (client->hostname = fstrread(fp)) == NULL)
		goto failed;

	if (fread(&n, sizeof(n), 1, fp) != 1)
		goto failed;

	if ((props = calloc(n, sizeof(*props))) == NULL)
		goto failed;

	for (pe = props + n, pp = props; pp < pe; pp++) {
		if ((*pp = malloc(sizeof(**pp))) == NULL)
			goto failed2;
		
		if (((*pp)->name = fstrread(fp)) == NULL ||
		    ((*pp)->type = fstrread(fp)) == NULL)
			goto failed2;

		if (fread(&m, sizeof(m), 1, fp) != 1)
			goto failed2;

		if ((values = calloc(m, sizeof(*values))) == NULL)
			goto failed2;

		for (ve = values + m, vp = values; vp < ve; vp++) {
			if (fread(&l, sizeof(l), 1, fp) != 1)
				goto failed2;

			/*
			 * allocate (l+1) bytes of memory, so ARRAY8
			 * values are garantied to be zero-terminated
			 */
			if ((vp->value = calloc(l + 1, 1)) == NULL)
				goto failed2;

			if (fread(vp->value, 1, l, fp) != l)
				goto failed2;

			vp->length = l;
		}

		(*pp)->num_vals = m;
		(*pp)->vals = values;
	}

	client->nProps = n;
	client->props = props;

	return(client);
	
failed2:
	for (; pp > props; pp--)
		SmFreeProperty(*pp);
	free(props);

failed:
	client_free(client);
	return(NULL);
}

/*
 */
static SmProp *
client_get_property(const Client *client, const gchar *name)
{
	SmProp **pe, **pp;

	if (client->props == NULL)
		return(NULL);

	pe = client->props + client->nProps;

	for (pp = client->props; pp < pe; pp++)
		if (!strcmp((*pp)->name, name))
			return(*pp);

	return(NULL);
}

/*
 */
gchar *
client_get_current_directory(const Client *client)
{
	SmProp *p;

	if ((p = client_get_property(client, SmCurrentDirectory)) != NULL)
		return(g_strdup(p->vals->value));

	return(NULL);
}

/*
 */
gchar *
client_get_program(const Client *client)
{
	SmProp *p;

	if ((p = client_get_property(client, SmProgram)) != NULL)
		return(g_strdup(p->vals->value));

	return(NULL);
}

/*
 */
gchar *
client_get_userid(const Client *client)
{
	SmProp *p;

	if ((p = client_get_property(client, SmUserID)) != NULL)
		return(g_strdup(p->vals->value));

	return(NULL);
}

/*
 */
gchar **
client_get_restart_command(const Client *client)
{
	SmProp *p;
	gchar **v;
	gint n;

	if ((p = client_get_property(client, SmRestartCommand)) != NULL) {
		v = g_new0(gchar *, p->num_vals + 1);

		for (n = 0; n < p->num_vals; n++)
			v[n] = g_strdup((gchar *)(p->vals[n].value));

		return(v);
	}
	
	return(NULL);
}

/*
 */
gchar **
client_get_command(const Client *client, const gchar *command)
{
	SmProp *p;
	gchar **v;
	gint n;

	if ((p = client_get_property(client, command)) != NULL) {
		if (strcmp(p->type, "LISTofARRAY8") == 0) {
			v = g_new0(gchar *, p->num_vals + 1);

			for (n = 0; n < p->num_vals; n++)
				v[n] = g_strdup((gchar *)(p->vals[n].value));
		
			return(v);
		}
	}
	
	return(NULL);
}

/* XXX */
static GQuark clientErrorDomain = 0;

#define	CLIENT_ERROR_NOSUCHCOMMAND	1

/*
 */
gboolean
client_run_command(const Client *client, const gchar *command, GError **error)
{
	gboolean result;
	gchar **argv;
	gchar *cwd;

	if (clientErrorDomain == 0)
		clientErrorDomain = g_quark_from_string("ClientErrorDomain");

	if ((argv = client_get_command(client, command)) == NULL) {
		if (error != NULL) {
			*error = g_error_new(clientErrorDomain,
				CLIENT_ERROR_NOSUCHCOMMAND,
				"Client %s has no command named %s",
				client->id, command);
		}
		return(FALSE);
	}

	cwd = client_get_current_directory(client);

	result = g_spawn_async(cwd, argv, NULL, G_SPAWN_SEARCH_PATH, NULL,
			NULL, NULL, error);

	if (cwd != NULL)
		g_free(cwd);
	g_strfreev(argv);

	return(result);
}

/*
 */
gint
client_get_priority(const Client *client)
{
	SmProp *p;

	if ((p = client_get_property(client, "_GSM_Priority")) != NULL)
		return((gint)*(guint8 *)p->vals->value);

	return(50);
}

/*
 */
gint
client_get_restart_style_hint(const Client *client)
{
	SmProp *p;

	if ((p = client_get_property(client, SmRestartStyleHint)) != NULL)
		return((gint)*(guint8 *)p->vals->value);

	return(SmRestartIfRunning);
}

/*
 */
gint
client_compare_priority(const Client *a, const Client *b)
{
	return(client_get_priority(a) - client_get_priority(b));
}

/*
 */
void
client_kill(const Client *client)
{
	g_return_if_fail(client != NULL);

	SmsDie(client->smsConn);
}

