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

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <X11/ICE/ICElib.h>
#include <X11/ICE/ICEutil.h>
#include <X11/SM/SMlib.h>

#include <glib.h>
#include <libxfce4util/i18n.h>

#include <xfce4-session/client.h>
#include <xfce4-session/session-control.h>
#include <xfce4-session/xfce_trayicon.h>
#include "ice-layer.h"
#include "manager.h"

/* prototypes */
static void	ice_error_handler(IceConn);
static gboolean	ice_process_messages(GIOChannel *, GIOCondition, IceConn);
static gboolean	ice_connection_accept(GIOChannel *, GIOCondition, IceListenObj);
static FILE 	*ice_tmpfile(char **);
static void	ice_auth_add(FILE *, FILE *, char *, IceListenObj);
static void	ice_cleanup(void);

/* ICE authority cleanup file name */
static char *authCleanupFile;

/*
 * ICE host based authentication proc
 */
Bool
ice_auth_proc(char *hostname)
{
	return(False);
}

/*
 * ICE I/O error handler
 */
static void
ice_error_handler(IceConn iceConn)
{
	/*
	 * The I/O error handlers does whatever is necessary to respond
	 * to the I/O error and then returns, but it does not call
	 * IceCloseConnection. The ICE connection is given a "bad IO"
	 * status, and all future reads and writes to the connection
	 * are ignored. The next time IceProcessMessages is called it
	 * will return a status of IceProcessMessagesIOError. At that
	 * time, the application should call IceCloseConnection.
	 */
	g_warning("ICE I/O error on connection %p", iceConn);
}

/*
 * Process ICE messages
 */
static gboolean
ice_process_messages(GIOChannel *channel, GIOCondition condition,
                     IceConn iceConn)
{
	/* XXX */
	extern GtkWidget *sessionControl;
	extern XfceTrayIcon *trayIcon;
	IceProcessMessagesStatus status;
	SmsConn smsConn;
	gchar *tip;
	GList *lp;

	status = IceProcessMessages(iceConn, NULL, NULL);

	if (status == IceProcessMessagesIOError) {
		for (lp = g_list_first(clients); lp; lp = lp->next)
			if (CLIENT(lp->data)->iceConn == iceConn)
				break;

		if (lp != NULL) {
			smsConn = CLIENT(lp->data)->smsConn;
			xfsm_session_control_remove(
					XFSM_SESSION_CONTROL(sessionControl),
					CLIENT(lp->data));
			client_free(CLIENT(lp->data));
			SmsCleanUp(smsConn);
			clients = g_list_delete_link(clients, lp);
		}

		/* update the tray icon tip */
		tip = g_strdup_printf(_("%u clients connected"),
				g_list_length(clients));
		xfce_tray_icon_set_tooltip(trayIcon, tip, NULL);
		g_free(tip);

		IceSetShutdownNegotiation(iceConn, False);
		(void)IceCloseConnection(iceConn);

		/* remove the I/O watch */
		return(FALSE);
	}

	/* keep the I/O watch running */
	return(TRUE);
}

/*
 * ICE connection watch. This is called whenever a new ICE connection is
 * made. It arranges for the ICE connection to be handled via the GLib
 * main event loop.
 */
/* ARGSUSED */
static void
ice_connection_watch(IceConn iceConn, IcePointer clientData, Bool opening,
                     IcePointer *watchData)
{
	GIOChannel *channel;
	guint watchID;
	gint fd;

	if (opening) {
		fd = IceConnectionNumber(iceConn);

		/*
		 * Make sure we don't pass on these file descriptors to an
		 * exec'd child process.
		 */
		fcntl(fd, F_SETFD, fcntl(fd, F_GETFD, 0) | FD_CLOEXEC);

		/* create an I/O channel for the new client connection */
		channel = g_io_channel_unix_new(fd);
		watchID = g_io_add_watch(channel, G_IO_ERR | G_IO_HUP | G_IO_IN,
				(GIOFunc)ice_process_messages, iceConn);
		g_io_channel_unref(channel);

		/* */
		*watchData = (IcePointer)GUINT_TO_POINTER(watchID);
	}
	else {
		/* remove the I/O source as the connection is going down */
		g_source_remove(GPOINTER_TO_UINT(*watchData));
	}
}

/*
 * Accept new ICE connections
 */
static gboolean
ice_connection_accept(GIOChannel *channel, GIOCondition condition,
                     IceListenObj iceListener)
{
	IceConnectStatus cstatus;
	IceAcceptStatus astatus;
	IceConn iceConn;

	iceConn = IceAcceptConnection(iceListener, &astatus);

	if (astatus != IceAcceptSuccess) {
		g_warning("Failed to accept ICE connection on listener %p",
				(void *)iceListener);
		return(TRUE);
	}

	/* Wait for the connection to leave pending state */
	do {
#if 0 /* THIS MIGHT CAUSE A RACE CONDITIION ?!?! */
		(void)g_main_context_iteration(NULL, TRUE);
#else
		(void)IceProcessMessages(iceConn, NULL, NULL);
#endif
	} while ((cstatus = IceConnectionStatus(iceConn)) == IceConnectPending);

	if (cstatus != IceConnectAccepted) {
		if (cstatus == IceConnectIOError) {
			g_warning("I/O error opening ICE connection %p",
				(void *)iceConn);
		}
		else {
			g_warning("ICE connectio %p rejected", (void *)iceConn);
		}

		IceSetShutdownNegotiation(iceConn, False);
		(void)IceCloseConnection(iceConn);
	}

	/* keep the listener watch running */
	return(TRUE);
}

/*
 */
static FILE *
ice_tmpfile(char **name)
{
	GError *error;
	mode_t mode;
	FILE *fp;
	int fd;

	fp = NULL;
	mode = umask(0077);

	if ((fd = g_file_open_tmp(".xfsm-ICE-XXXXXX", name, &error)) < 0) {
		g_warning("Unable to open temporary file: %s", error->message);
		g_error_free(error);
	}
	else
		fp = fdopen(fd, "wb");

	umask(mode);

	return(fp);
}

/*
 * for printing hex digits (taken from KDE3)
 */
static void
fprintfhex(FILE *fp, int len, char *cp)
{
	static char hexchars[] = "0123456789abcdef";

	for (; len > 0; len--, cp++) {
		unsigned char s = *cp;
		putc(hexchars[s >> 4], fp);
		putc(hexchars[s & 0x0f], fp);
	}
}

/*
 */
static void
ice_auth_add(FILE *setupFp, FILE *cleanupFp, char *protocol,
             IceListenObj iceListener)
{
	IceAuthDataEntry entry;

	entry.protocol_name = protocol;
	entry.network_id = IceGetListenConnectionString(iceListener);
	entry.auth_name = "MIT-MAGIC-COOKIE-1";
	entry.auth_data = IceGenerateMagicCookie(16);
	entry.auth_data_length = 16;

	IceSetPaAuthData(1, &entry);

	fprintf(setupFp, "add %s \"\" %s MIT-MAGIC-COOKIE-1 ", protocol,
			entry.network_id);
	fprintfhex(setupFp, 16, entry.auth_data);
	fprintf(setupFp, "\n");

	fprintf(cleanupFp, "remove protoname=%s protodata=\"\" netid=%s "
			   "authname=MIT-MAGIC-COOKIE-1\n", protocol,
			   entry.network_id);

	free(entry.network_id);
	free(entry.auth_data);
}

/*
 */
gboolean
ice_setup_listeners(int numListeners, IceListenObj *listenObjs)
{
	GIOChannel *channel;
	char *authSetupFile;
	gchar *command;
	FILE *cleanupFp;
	FILE *setupFp;
	int fd;
	int n;

	/* */
	IceSetIOErrorHandler(ice_error_handler);
	IceAddConnectionWatch(ice_connection_watch, NULL);

	if ((cleanupFp = ice_tmpfile(&authCleanupFile)) == NULL)
		return(FALSE);

	if ((setupFp = ice_tmpfile(&authSetupFile)) == NULL) {
		(void)fclose(cleanupFp);
		(void)unlink(authCleanupFile);
		g_free(authCleanupFile);
		return(FALSE);
	}

	for (n = 0; n < numListeners; n++) {
		fd = IceGetListenConnectionNumber(listenObjs[n]);

		/*
		 * Make sure we don't pass on these file descriptors to an
		 * exec'd child process.
		 */
		fcntl(fd, F_SETFD, fcntl(fd, F_GETFD, 0) | FD_CLOEXEC);

		/* create an I/O channel for the new client connection */
		channel = g_io_channel_unix_new(fd);
		(void)g_io_add_watch(channel, G_IO_ERR | G_IO_HUP | G_IO_IN,
				(GIOFunc)ice_connection_accept,
				(gpointer)listenObjs[n]);
		g_io_channel_unref(channel);

		/* setup auth for this listener */
		ice_auth_add(setupFp, cleanupFp, "ICE", listenObjs[n]);
		ice_auth_add(setupFp, cleanupFp, "XSMP", listenObjs[n]);
		IceSetHostBasedAuthProc(listenObjs[n], ice_auth_proc);
	}

	/* close ICE authority files */
	(void)fclose(setupFp);
	(void)fclose(cleanupFp);

	/* setup ICE authority */
	command = g_strdup_printf("iceauth source %s", authSetupFile);
	system(command);
	g_free(command);

	/* remove the setup file, no longer needed */
	unlink(authSetupFile);
	g_free(authSetupFile);

	/* remember to cleanup ICE stuff on exit() */
	g_atexit(ice_cleanup);

	return(TRUE);
}

/*
 * Cleanup ICE stuff
 */
void
ice_cleanup(void)
{
	gchar *command;

	g_return_if_fail(authCleanupFile != NULL);

	/* remove newly added ICE authority entries */
	command = g_strdup_printf("iceauth source %s", authCleanupFile);
	system(command);
	g_free(command);

	/* remove the cleanup file, no longer needed */
	unlink(authCleanupFile);
	g_free(authCleanupFile);
}


