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

#include <X11/ICE/ICElib.h>
#include <X11/ICE/ICEutil.h>
#include <X11/SM/SMlib.h>
#include <X11/Xlib.h>

#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/netk-util.h>
#include <gtk/gtk.h>

#include <xfce4-session/client.h>
#include <xfce4-session/manager.h>
#include <xfce4-session/splash-screen.h>
#include <xfce4-session/startup.h>

/* list of clients to restore */
GList	*pendingClients = NULL;

/* */
static guint	pendingTimeoutId = 0;

/* */
static GtkWidget	*splash = NULL;

/* */
gchar		*startupSplashTheme = NULL;

/* */
#define PENDING_TIMEOUT		(5 * 1000)

/*
 */
static gboolean
destroy_splash(void)
{
	gtk_widget_destroy(splash);
	return(FALSE);
}

/*
 * Run ~/Desktop/Autostart/ stuff
 */
static gboolean
pending_Autostart(void)
{
	const gchar *entry;
	GError *error;
	guint timer;
	gchar *argv[] = { NULL, NULL };
	gchar *dir;
	GDir *dp;

	dir = xfce_get_homefile("Desktop", "Autostart", NULL);
	timer = 0;

	if ((dp = g_dir_open(dir, 0, NULL)) != NULL) {
		while ((entry = g_dir_read_name(dp)) != NULL) {
			*argv = g_build_filename(dir, entry, NULL);
			error = NULL;

			if (g_spawn_async(NULL, argv, NULL, 0, NULL, NULL, NULL,
						&error)) {
				timer++;
			}
			else {
				g_warning("Unable to launch %s: %s", *argv,
						error->message);
				g_error_free(error);
			}

			g_free(*argv);
		}

		g_dir_close(dp);
	}

	g_free(dir);

	if (timer > 0)
		g_timeout_add(timer * 400, (GSourceFunc)destroy_splash, NULL);
	else
		destroy_splash();

	return(FALSE);
}

/*
 */
static gboolean
pending_timeout(Client *client)
{
	/* client startup timed out */
	if (client != NULL)
		g_warning("client %s timed out", client->id);
	pending_continue(client);
	return(FALSE);
}

/*
 */
static gboolean
pending_launch(void)
{
	Client *client;
	GError *error;
	gchar *program;

	for (;;) {
		/* REQUIRED! */
		error = NULL;

		/* check if we are finished */
		if (g_list_length(pendingClients) == 0) {
			/*
			 * All session aware apps are up and running,
			 * launch the Autostart stuff.
			 */
			xfsm_splash_screen_set_text(XFSM_SPLASH_SCREEN(splash),
					_("Doing Autostart..."));

			g_idle_add((GSourceFunc)pending_Autostart, NULL);
			break;
		}

		/* get next client to fire up */
		client = CLIENT(g_list_first(pendingClients)->data);

		/* try to start client */
		if (!client_run_command(client, SmRestartCommand, &error)) {
			g_warning("Unable to start client %s: %s", client->id,
					error->message);
			g_error_free(error);

			/* run DiscardCommand */
			client_run_command(client, SmDiscardCommand, NULL);

			/* remove client */
			pendingClients = g_list_remove(pendingClients, client);
			client_free(client);

			/* try next client */
			continue;
		}

		/* notify the user of what we are doing */
		if ((program = client_get_program(client)) != NULL) {
			xfsm_splash_screen_launch(XFSM_SPLASH_SCREEN(splash),
					program);
			g_free(program);
		}

		/* add startup timeout */
		pendingTimeoutId = g_timeout_add(PENDING_TIMEOUT,
				(GSourceFunc)pending_timeout, client);
		break;
	}

	return(FALSE);
}

/*
 */
void
pending_continue(Client *client)
{
	if (pendingTimeoutId != 0)
		g_source_remove(pendingTimeoutId);

	if (client != NULL) {
		pendingClients = g_list_remove(pendingClients, client);
		client_free(client);
	}
	else if (splash == NULL) {
		/* Ok, start up the splash screen */
		splash = xfsm_splash_screen_new(startupSplashTheme,
				g_list_length(pendingClients),
				_("Starting session manager..."));

		gtk_widget_show(splash);

		/* XXX - give time to the splash screen to appear */
		(void)g_timeout_add(100, (GSourceFunc)pending_timeout, NULL);
		return;
	}

	(void)g_timeout_add(100, (GSourceFunc)pending_launch, NULL);
}


