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
 *
 * TODO:
 *
 *	- Fix Linux weirdness with utsname.machine
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <X11/ICE/ICElib.h>
#include <X11/ICE/ICEutil.h>
#include <X11/SM/SMlib.h>

#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/dialogs.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "client.h"
#include "ice-layer.h"
#include "manager.h"
#include "startup.h"
#include "shutdown.h"
#include "util.h"

#include <xfce4-session/session-control.h>
#include <xfce4-session/xfce_trayicon.h>

#define XFSM_VERSION	2

/* current manager status */
int state = MANAGER_IDLE;

/* list of currently connected SM clients */
GList	*clients = NULL;

/* session filename */
gchar	*sessionFile = NULL;

/* type of shutdown to do */
gint		shutdownType = SHUTDOWN_LOGOUT;
gboolean	shutdownSave = TRUE;

/* id of running "Die" timeout if any */
static guint	dieTimeoutId = 0;

/* ICE socket listener objects */
static int numListeners;
static IceListenObj *listenObjs;

/* client list GUI */
GtkWidget	*sessionControl = NULL;

/* system tray icon */
extern XfceTrayIcon	*trayIcon;

/* prototypes */
static Status	new_client(SmsConn, SmPointer, unsigned long *, SmsCallbacks *,
                           char **);
static Status	register_client(SmsConn, Client *, char *);
static void	interact_request(SmsConn, Client *, int);
static void	interact_done(SmsConn, Client *, Bool);
static void	save_yourself_request(SmsConn, Client *, int, Bool, int, Bool,
                                      Bool);
static void	save_yourself_phase2_request(SmsConn, Client *);
static void	save_yourself_done(SmsConn, Client *, Bool);
static void	close_connection(SmsConn, Client *, int, char **);
static void	set_properties(SmsConn, Client *, int, SmProp **);
static void	delete_properties(SmsConn, Client *, int, char **);
static void	get_properties(SmsConn, Client *);

#define CALLBACK(_callbacks, _callback, _client)			\
do {									\
	(void *)_callbacks->_callback.callback = (void *)_callback;	\
	(void *)_callbacks->_callback.manager_data = (void *)_client;	\
} while (0)

/*
 * Convince macro
 */
#define	client_set_state(_client, _state)				\
do {									\
	CLIENT((_client))->state = _state;				\
	xfsm_session_control_update(XFSM_SESSION_CONTROL(sessionControl),\
			CLIENT((_client)));				\
} while (0)

/*
 * Provide a setenv function for systems that lack it
 */
#ifndef HAVE_SETENV
static void
setenv(const gchar *name, const gchar *value, gboolean overwrite)
{
	gchar *buf;

	buf = g_strdup_printf("%s=%s", name, value);
	putenv(buf);
	g_free(buf);
}
#endif	/* !HAVE_SETENV */

/*
 */
gboolean
manager_init(gboolean disable_tcp)
{
	char *sessionManager;
	char error[2048];

  /* check if theres already a session manager running */
  if (getenv("SESSION_MANAGER") != NULL) {
    g_warning("Another session manager is already running, unable to continue");
    return(FALSE);
  }

	if (!SmsInitialize(PACKAGE, VERSION, new_client, NULL, ice_auth_proc,
				2048, error)) {
		g_warning("Unable to register XSM protocol: %s", error);
		return(FALSE);
	}

#ifdef HAVE__ICETRANSNOLISTEN
  if (disable_tcp) {
    extern void _IceTransNoListen(char *);
    _IceTransNoListen("tcp");
  }
#endif

	if (!IceListenForConnections(&numListeners, &listenObjs, 2048,
			error)) {
		g_error("Unable to listen for ICE connections: %s", error);
		return(FALSE);
	}

	/* setup ICE authentication layer */
	ice_setup_listeners(numListeners, listenObjs);

	/* set SESSION_MANAGER environment variable */
	sessionManager = IceComposeNetworkIdList(numListeners, listenObjs);
	setenv("SESSION_MANAGER", sessionManager, TRUE);
	free(sessionManager);

	/* XXX */
	sessionControl = xfsm_session_control_new();

	return(TRUE);
}

/*
 */
gboolean
manager_save(void)
{
	struct utsname name;
	GList *lp;
	FILE *fp;
	int n, v;

	g_return_val_if_fail(sessionFile != NULL, FALSE);

	if ((n = g_list_length(clients)) == 0) {
		/* nothing to save */
		(void)unlink(sessionFile);
		return(TRUE);
	}

	/* get system information */
	if (uname(&name) < 0) {
		g_warning("Unable to retrieve system information: %s",
				g_strerror(errno));
		return(FALSE);
	}

	if ((fp = fopen(sessionFile, "wb")) == NULL) {
		g_warning("Unable to open session file %s for writing: %s",
				sessionFile, g_strerror(errno));
		return(FALSE);
	}

	/* write version */
	v = XFSM_VERSION;
	if (fwrite(&v, sizeof(v), 1, fp) != 1)
		goto end;

	/* write system name */
	fstrwrite(fp, name.sysname);

	/* write machine hardware plattform */
	fstrwrite(fp, name.machine);

	/* write number of clients */
	if (fwrite(&n, sizeof(n), 1, fp) != 1)
		goto end;

	/* save all active clients */
	for (lp = g_list_first(clients); lp; lp = lp->next) {
		Client *client = CLIENT(lp->data);
		gchar **argv;

		/*
		 * Do not save clients that set SmRestartStyleHint to
		 * SmRestartNever.
		 */
		if (client_get_restart_style_hint(client) == SmRestartNever)
			continue;

		/*
		 * Save only clients that have the SmRestartCommand
		 * property set.
		 */
		if ((argv = client_get_restart_command(client)) == NULL) {
			g_warning("Client %s has no SmRestartCommand set",
					client->id);
			continue;
		}
		g_strfreev(argv);

		if (!client_save(client, fp) && ferror(fp))
			break;
	}

end:
	n = !ferror(fp);
	(void)fclose(fp);
	return(n);
}

/*
 * This does the initial startup. Starting up have to be done after atleast
 * one run through the gtk main loop, else the splash screen will use the
 * old screen height/width that was set before the mcs manager (with the
 * Xrandr plugin) fired up.
 */
static gboolean
manager_startup(void)
{
	/* sort list of pending clients by priority */
	pendingClients = g_list_sort(pendingClients,
		(GCompareFunc)client_compare_priority);

	pending_continue(NULL);

	return(FALSE);
}

/*
 */
gboolean
manager_restart(void)
{
	gchar *sysname;
	gchar *machine;
	gchar *hostname;
	struct utsname name;
	Client *client;
	gchar *file;
	FILE *fp;
	int n;

	g_return_val_if_fail(sessionFile == NULL, FALSE);

	hostname = xfce_gethostname();
	file = g_strdup_printf("xfsm-%s", hostname);
	sessionFile = xfce_get_userfile("sessions", file, NULL);
	g_free(file);
	g_free(hostname);

	/* get system information */
	if (uname(&name) < 0) {
		g_warning("Unable to retrieve system information: %s",
				g_strerror(errno));
		return(FALSE);
	}

	if ((fp = fopen(sessionFile, "rb")) == NULL) {
		g_warning("Unable to open session file %s for reading: %s",
				sessionFile, g_strerror(errno));
		return(FALSE);
	}

	/* read version */
	if (fread(&n, sizeof(n), 1, fp) != 1)
		goto end;

	/* version < 2 does not have system information */
	if (n > 1) {
		if ((sysname = fstrread(fp)) == NULL ||
		    (machine = fstrread(fp)) == NULL)
			goto end;

		/* check if system and machine equals */
		if (strcmp(sysname, name.sysname) != 0 ||
		    strcmp(machine, name.machine) != 0) {
			g_warning("Session file %s was written on a different "
				  "machine", sessionFile);
			goto end;
		}
	}

	/* read number of clients */
	if (fread(&n, sizeof(n), 1, fp) != 1 || n < 1)
		goto end;

	while (n-- > 0) {
		if ((client = client_load(fp)) == NULL)
			goto end;

		pendingClients = g_list_append(pendingClients, client);
	}

	(void)fclose(fp);

	(void)g_idle_add((GSourceFunc)manager_startup, NULL);

	return(TRUE);

end:
	(void)fclose(fp);

	/* notify the user that we were unable to restore the session */
	xfce_err(_(
		"The session manager was unable to restore your\n"
		"previous session. It'll therefore start a default\n"
		"session."));

	return(FALSE);
}

/*
 * Safe way to generate a client id, in case SmsGenerateClientID() fails
 */
char *
manager_generate_client_id(SmsConn smsConn)
{
	static char *addr = NULL;
	static int sequence = 0;
	char *id;

	if (smsConn == NULL || (id = SmsGenerateClientID(smsConn)) == NULL) {
		if (addr == NULL) {
			/*
			 * Faking our IP address, the 0 below is "unknown"
			 * address format (1 would be IP, 2 would be DEC-NET
			 * format). Stolen from KDE :-)
			 */
			addr = g_strdup_printf("0%.8x", g_random_int());
		}

		id = (char *)malloc(50);
		(void)g_snprintf(id, 50, "1%s%.13ld%.10d%.4d", addr,
				 (long)time(NULL), (int)getpid(), sequence);
		sequence = (sequence + 1) % 10000;
	}

	return(id);
}

/*
 */
void
manager_saveyourself(int saveType, Bool shutdown, int interactStyle, Bool fast)
{
	GList *lp;

	g_return_if_fail(state == MANAGER_IDLE);

	shutdownSave = TRUE;

	/* ask user whether to logout */
	if (!fast && shutdown && !shutdownDialog(&shutdownType, &shutdownSave))
		return;

	/* */
	if (!shutdown || shutdownSave) {
		/* */
		state = shutdown ? MANAGER_SHUTDOWN : MANAGER_CHECKPOINT;

		/* put all clients into SaveYourself state */
		for (lp = g_list_first(clients); lp; lp = lp->next) {
			if (client_state(lp->data) == CLIENT_SAVINGLOCAL) {
				/*
				 * client is already in saving state, no need
				 * to send another SaveYourself message.
				 */
				CLIENT(lp->data)->state = CLIENT_SAVING;
				continue;
			}
			
			client_set_state(lp->data, CLIENT_SAVING);
			SmsSaveYourself(CLIENT(lp->data)->smsConn, saveType,
					shutdown, interactStyle, fast);
		}
	}
	else {
		/*
		 * We are about to shutdown the manager without saving, so
		 * send all SM clients the SmDie message.
		 */
		state = MANAGER_SHUTDOWNPHASE2;
		for (lp = g_list_first(clients); lp; lp = lp->next)
			SmsDie(CLIENT(lp->data)->smsConn);

		/* give all clients the chance to close the connection */
		dieTimeoutId = g_timeout_add(5 * 1000,
			(GSourceFunc)gtk_main_quit, NULL);
	}
}

/*
 */
static Status
new_client(SmsConn smsConn, SmPointer managerData, unsigned long *mask,
           SmsCallbacks *callbacks, char **failureReason)
{
	Client *client;

	client = client_new(smsConn);

	CALLBACK(callbacks, register_client, client);
	CALLBACK(callbacks, interact_request, client);
	CALLBACK(callbacks, interact_done, client);
	CALLBACK(callbacks, save_yourself_request, client);
	CALLBACK(callbacks, save_yourself_phase2_request, client);
	CALLBACK(callbacks, save_yourself_done, client);
	CALLBACK(callbacks, close_connection, client);
	CALLBACK(callbacks, set_properties, client);
	CALLBACK(callbacks, delete_properties, client);
	CALLBACK(callbacks, get_properties, client);

	*mask = SmsRegisterClientProcMask | SmsInteractRequestProcMask |
		SmsInteractDoneProcMask | SmsSaveYourselfRequestProcMask |
		SmsSaveYourselfP2RequestProcMask | SmsSaveYourselfDoneProcMask |
		SmsCloseConnectionProcMask | SmsSetPropertiesProcMask |
		SmsDeletePropertiesProcMask | SmsGetPropertiesProcMask;

	return(True);
}

/*
 */
static Status
register_client(SmsConn smsConn, Client *client, char *previousId)
{
	gchar *tip;
	GList *lp;

	if (previousId != NULL) {
		for (lp = g_list_first(pendingClients); lp; lp = lp->next)
			if (!strcmp(CLIENT(lp->data)->id, previousId))
				break;

		/*
		 * If previous-ID is not valied, the SM will send a BadValue
		 * error message to the client. At this point the SM reverts
		 * to the register state and waits for another RegisterClient.
		 */
		if (lp == NULL)
			return(False);

		/* set initial properties */
		client->nProps = CLIENT(lp->data)->nProps;
		client->props = CLIENT(lp->data)->props;
		client->id = previousId;

		/* remove pending client data */
		CLIENT(lp->data)->props = NULL;
		pending_continue(CLIENT(lp->data));
	}
	else {
		/* generate a new client id */
		client->id = manager_generate_client_id(smsConn);

		/* previous-ID is no longer needed */
		free(previousId);
	}

	/* send register reply to client */
	SmsRegisterClientReply(smsConn, client->id);

	/* */
	client->hostname = SmsClientHostName(smsConn);

	/* add client to the list of connected clients */
	clients = g_list_append(clients, client);

	/*
	 * If the client didn't supply a previous-ID field to the
	 * RegisterClient message, the SM must send a SaveYourself
	 * message with type = Local, shutdown = False, interact-style
	 * = None and fast = False immediatly after the RegisterClientReply.
	 * The client should respond to this like any other SaveYourself
	 * message.
	 */
	if (previousId == NULL) {
		client->state = CLIENT_SAVINGLOCAL;
		SmsSaveYourself(smsConn, SmSaveLocal, False,
				SmInteractStyleNone, False);
	}

	/* XXX */
	xfsm_session_control_append(XFSM_SESSION_CONTROL(sessionControl),
			client);

	/* update the tray icon tooltip */
	tip = g_strdup_printf(_("%u clients connected"),
			g_list_length(clients));
	xfce_tray_icon_set_tooltip(trayIcon, tip, NULL);
	g_free(tip);

	return(True);
}

/*
 */
static void
interact_request(SmsConn smsConn, Client *client, int dialogType)
{
	GList *lp;

	g_return_if_fail(state == MANAGER_CHECKPOINT ||
			 state == MANAGER_SHUTDOWN);
	g_return_if_fail(client->state == CLIENT_SAVING);

	for (lp = g_list_first(clients); lp; lp = lp->next) {
		if (client_state(lp->data) == CLIENT_INTERACTING) {
			/*
			 * Theres already a client interacting, so put this
			 * one on the wait queue
			 */
			client_set_state(client, CLIENT_WAITFORINTERACT);
			return;
		}
	}

	/* let this client interact with the user */
	client_set_state(client, CLIENT_INTERACTING);
	SmsInteract(smsConn);
}

/*
 */
static void
interact_done(SmsConn smsConn, Client *client, Bool cancelShutdown)
{
	GList *lp;

	g_return_if_fail(state == MANAGER_CHECKPOINT ||
			 state == MANAGER_SHUTDOWN);
	g_return_if_fail(client_state(client) == CLIENT_INTERACTING);

	client_set_state(client, CLIENT_SAVING);

	/*
	 * Setting the cancel-shutdown field to True indicates that the user
	 * has requested that the entire shutdown be cancelled. Cancel-
	 * shutdown may only be True if the corresponding SaveYourself
	 * message specified True for the shutdown field and Any or Error
	 * for the interact-style field. Otherwise, cancel-shutdown must be
	 * False.
	 */
	if (cancelShutdown && state == MANAGER_SHUTDOWN) {
		/* we go into checkpoint state from here... */
		state = MANAGER_CHECKPOINT;

		/* 
		 * If a shutdown is in progress, the user may have the option
		 * of cancelling the shutdown. If the shutdown is cancelled
		 * (specified in the "Interact Done" message), the session
		 * manager should send a "Shutdown Cancelled" message to each
		 * client that requested to interact.
		 */
		for (lp = g_list_first(clients); lp; lp = lp->next) {
			if (client_state(lp->data) != CLIENT_WAITFORINTERACT)
				continue;

			/* reset all clients that are waiting for interact */
			client_set_state(client, CLIENT_SAVING);
			SmsShutdownCancelled(CLIENT(lp->data)->smsConn);
		}

		return;
	}

	/* Let the next client interact */
	for (lp = g_list_first(clients); lp; lp = lp->next) {
		if (client_state(lp->data) == CLIENT_WAITFORINTERACT) {
			client_set_state(lp->data, CLIENT_INTERACTING);
			SmsInteract(CLIENT(lp->data)->smsConn);
			break;
		}
	}
}

/*
 */
static void
save_yourself_request(SmsConn smsConn, Client *client, int saveType,
                      Bool shutdown, int interactStyle, Bool fast, Bool global)
{
	g_return_if_fail(client_state(client) == CLIENT_IDLE);

	if (!global) {
		/*
		 * client requests a local checkpoint. We slightly ignore
		 * shutdown here, since it does not make sense for a local
		 * checkpoint.
		 */
		client_set_state(client, CLIENT_SAVINGLOCAL);
		SmsSaveYourself(smsConn, saveType, FALSE, interactStyle, fast);
	}
	else {
		/* request the manager to do a SaveYourself */
		manager_saveyourself(saveType, shutdown, interactStyle, fast);
	}
}

/*
 */
static void
save_yourself_phase2_request(SmsConn smsConn, Client *client)
{
	GList *lp;

	if (state != MANAGER_CHECKPOINT && state != MANAGER_SHUTDOWN) {
		/*
		 * If we are not in checkpoint or shutdown state, just
		 * send the save yourself phase2.
		 */
		SmsSaveYourselfPhase2(client->smsConn);
		return;
	}

	client->state = CLIENT_WAITFORPHASE2;

	/* check if we are ready to enter Phase2 */
	for (lp = g_list_first(clients); lp; lp = lp->next) {
		if (client_state(lp->data) == CLIENT_SAVING) {
			/* still clients in SaveYourself state */
			return;
		}
	}

	/* ok, we are ready to enter SaveYourselfPhase2 */
	for (lp = g_list_first(clients); lp; lp = lp->next) {
		if (client_state(lp->data) == CLIENT_WAITFORPHASE2) {
			client_set_state(lp->data, CLIENT_SAVING);
			SmsSaveYourselfPhase2(CLIENT(lp->data)->smsConn);
		}
	}
}

/*
 */
static void
save_yourself_done(SmsConn smsConn, Client *client, Bool success)
{
	gboolean enterPhase2;
	GList *lp;

	/* A client completed a local SaveYourself */
	if (client_state(client) == CLIENT_SAVINGLOCAL) {
		client_set_state(client, CLIENT_IDLE);
		SmsSaveComplete(client->smsConn);
		return;
	}

	/* validate client requests */
	if (client_state(client) != CLIENT_SAVING) {
#ifdef DEBUG
		g_warning("The client %s which is neither in SaveYourself "
			  "nor in SaveYourselfPhase2 state send a "
			  "SaveYourselfDone message", client->id);
#endif
		return;
	}

	/* manager has to be in checkpoint or shutdown state here */
	g_return_if_fail(state == MANAGER_CHECKPOINT ||
			 state == MANAGER_SHUTDOWN);

	client_set_state(client, CLIENT_SAVEDONE);

	enterPhase2 = FALSE;

	/* check if all clients completed the SaveYourself */
	for (lp = g_list_first(clients); lp; lp = lp->next) {
		if (client_state(lp->data) == CLIENT_SAVING) {
			/*
			 * atleast one client did not finish the SaveYourself
			 * yet, so wait another round.
			 */
			return;
		}
		else if (CLIENT(lp->data)->state == CLIENT_WAITFORPHASE2)
			enterPhase2 = TRUE;
	}

	/*
	 * Ok, all clients completed the SaveYourself Phase1, and there's
	 * atleast one client that requested to enter save Phase2, so lets
	 * enter SaveYourselfPhase2.
	 */
	if (enterPhase2) {
		for (lp = g_list_first(clients); lp; lp = lp->next) {
			if (CLIENT(lp->data)->state != CLIENT_WAITFORPHASE2)
				continue;

			client_set_state(lp->data, CLIENT_SAVING);
			SmsSaveYourselfPhase2(CLIENT(lp->data)->smsConn);
		}

		return;
	}

	/*
	 * At this point all clients completed the SaveYourself, so we
	 * are now save to complete the CheckPoint (and shutdown the
	 * session manager).
	 */
	if (state == MANAGER_CHECKPOINT) {
		if (!manager_save())
			g_warning("Unable to save session checkpoint");

		/* Notify all clients that we passed the checkpoint */
		for (lp = g_list_first(clients); lp; lp = lp->next) {
#ifdef DEBUG
			if (CLIENT(lp->data)->state != CLIENT_SAVEDONE) {
				g_warning("Client %s survied in non "
					  "CLIENT_SAVEDONE state.",
					  CLIENT(lp->data)->id);
			}
#endif
			client_set_state(lp->data, CLIENT_IDLE);
			SmsSaveComplete(CLIENT(lp->data)->smsConn);
		}

		/* reset manager to idle state */
		state = MANAGER_IDLE;

		return;
	}

	/*
	 * We are about to shutdown the manager, so send all SM clients the
	 * SmDie message.
	 */
	state = MANAGER_SHUTDOWNPHASE2;
	for (lp = g_list_first(clients); lp; lp = lp->next)
		SmsDie(CLIENT(lp->data)->smsConn);

	/* give all clients the chance to close the connection */
	dieTimeoutId = g_timeout_add(5 * 1000, (GSourceFunc)gtk_main_quit,NULL);
}

/*
 */
static void
close_connection(SmsConn smsConn, Client *client, int nReasons, char **reasons)
{
	IceConn iceConn;
	gchar *program;
	gchar *reason;
	gchar *tip;
	GList *lp;

	/* XXX */
	xfsm_session_control_remove(XFSM_SESSION_CONTROL(sessionControl),
			client);

	/* shutdown the XSMP/ICE connection */
	iceConn = SmsGetIceConnection(smsConn);
	SmsCleanUp(smsConn);
	IceSetShutdownNegotiation(iceConn, False);
	IceCloseConnection(iceConn);

	/* remember this client to be in "Disconnect"-state, see below */
	client_set_state(client, CLIENT_DISCONNECTED);

	if (state == MANAGER_SHUTDOWNPHASE2) {
		/* Check if there are still clients connected to the manager */
		for (lp = g_list_first(clients); lp; lp = lp->next)
			if (CLIENT(lp->data)->state != CLIENT_DISCONNECTED)
				return;
		
		/* remove a running "Die" timeout */
		if (dieTimeoutId != 0)
			g_source_remove(dieTimeoutId);

		/* All clients disconnected before the timeout, nice */
		gtk_main_quit();
	}
	else {
		if ((program = client_get_program(client)) == NULL)
			program = client_get_id(client);

		/* we are not in frozen shutdown state, so save to remove */
		clients = g_list_remove(clients, client);
		client_free(client);

		if (nReasons) {
			reason = xfce_strjoin("\n", reasons, nReasons);

			xfce_err(_(
				"The client \"%s\" closed the connection\n"
				"to the session manager. The following reason\n"
				"was given:\n\n%s"), program, reason);

			g_free(reason);
		}

		g_free(program);
	}

	/* update the tray icon tooltip */
	tip = g_strdup_printf(_("%u clients connected"),
			g_list_length(clients));
	xfce_tray_icon_set_tooltip(trayIcon, tip, NULL);
	g_free(tip);

	if (nReasons)
		SmFreeReasons(nReasons, reasons);
}

/*
 */
static void
set_properties(SmsConn smsConn, Client *client, int nProps, SmProp **props)
{
	int i, j, n;

	if (client->props == NULL) {
		/* First time, SetProperties is called, just use the given
		 * properties
		 */
		client->nProps = nProps;
		client->props = props;
		return;
	}

	n = client->nProps;

	/* check new array size and update existing properties */
	for (j = 0; j < nProps; j++) {
		for (i = 0; i < n; i++) {
			if (!strcmp(client->props[i]->name, props[j]->name)) {
				/* update existing property */
				SmFreeProperty(client->props[i]);
				client->props[i] = props[j];
				props[j] = NULL;
				break;
			}
		}

		if (i == n)
			client->nProps++;
	}

	/* resize properties array */
	client->props = (SmProp **)realloc(client->props,
			client->nProps * sizeof(*props));

	/* add leftover properties */
	for (j = 0; j < nProps; j++)
		if (props[j] != NULL)
			client->props[n++] = props[j];

	free(props);

	/* update client list GUI */
	xfsm_session_control_update(XFSM_SESSION_CONTROL(sessionControl),
			client);
}

/*
 */
static void
delete_properties(SmsConn smsConn, Client *client, int numProps,
                  char **propNames)
{
	char **name;
	int n, m;

	if (client->props == NULL)
		return;

	for (n = m = 0; n < client->nProps; n++) {
		for (name = propNames; name < propNames + numProps; name++) {
			if (!strcmp(client->props[n]->name, *name)) {
				SmFreeProperty(client->props[n]);
				break;
			}
		}

		if (name >= propNames + numProps)
			client->props[m++] = client->props[n];
	}

	/* any properties left? */
	if ((client->nProps = m) == 0) {
		free(client->props);
		client->props = NULL;
	}

	/* free property names */
	for (name = propNames; name < propNames + numProps; name++)
		free(*name);
	free(propNames);

	/* update client list GUI */
	xfsm_session_control_update(XFSM_SESSION_CONTROL(sessionControl),
			client);
}

/*
 */
static void
get_properties(SmsConn smsConn, Client *client)
{
	if (client->props != NULL)
		SmsReturnProperties(smsConn, client->nProps, client->props);
}

