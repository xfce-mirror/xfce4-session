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
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include <ctype.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libxfce4util/i18n.h>
#include <glib.h>

/*
 * Check if a user is allowed to issue shutdown commands (when running
 * setuid to root)
 */
gboolean
checkUser(const gchar *action)
{
	gchar buffer[LINE_MAX];
	const gchar *username;
	const gchar *u;
	FILE *fp;
	gchar *p;

	g_return_val_if_fail(action != NULL, FALSE);

	if ((username = g_get_user_name()) == NULL)
		return(FALSE);

	if ((fp = fopen(PATH_SHUTDOWNALLOW, "r")) == NULL)
		return(FALSE);

	while (fgets(buffer, LINE_MAX, fp) != NULL) {
		/* skip leading white space */
		for (p = buffer; isspace(*p); p++);
		
		/* ignore comments and empty lines */
		if (*p == '#' || *p == '\n' || *p == '\0')
			continue;

		/* */
		for (u = username; *u != '\0'; )
			if (*u++ != *p++)
				break;

		if (*u == '\0' && (*p == '\0' || isspace(*p))) {
			/* ok, we found him, he's ok */
			(void)fclose(fp);
			return(TRUE);
		}
	}

	(void)fclose(fp);
	return(FALSE);
}

/*
 */
gboolean
sudo(const gchar *action)
{
	gboolean result;
	gchar *command;
	gchar *errbuf;
	gchar *outbuf;
	GError *err;
	gint status;

	err = NULL;
	command = g_strdup_printf("sudo -S -u root /sbin/%s", action);
	result = g_spawn_command_line_sync(command, &outbuf, &errbuf, &status,
			&err);
	g_free(command);

	if (!result) {
		g_error_free(err);
		return(FALSE);
	}

	return(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

/*
 */
int
main(int argc, char **argv)
{
	struct stat sb;
	char *action;

	/* */
	if (argc != 2) {
		fprintf(stderr, _("Usage: %s <action>\n"), *argv);
		return(EXIT_FAILURE);
	}

	action = argv[1];

	/* */
	if (strcmp(action, "halt") && strcmp(action, "poweroff") &&
	    strcmp(action, "reboot")) {
		fprintf(stderr, _("%s: Unknown action %s\n"), action, *argv);
		return(EXIT_FAILURE);
	}

	if (geteuid() == 0) {
		if (stat(PATH_SHUTDOWNALLOW, &sb) < 0) {
			fprintf(stderr, "Unable to stat file %s: %s\n",
					PATH_SHUTDOWNALLOW, g_strerror(errno));
			return(EXIT_FAILURE);
		}

		if (sb.st_uid != 0) {
			fprintf(stderr, "%s is not owned by root",
					PATH_SHUTDOWNALLOW);
			return(FALSE);
		}

		/*
		 * Ok, we are running as root (or setuid to root), so lets
		 * try to do it the simple way
		 */
		if (getuid() && !checkUser(action)) {
			fprintf(stderr,	_(
			"You are not allowed to execute the action \"%s\".\n"
			"Ask your system adminitrator to add you to the list\n"
			"of people allowed to execute shutdown actions, by\n"
			"adding your username to the file %s.\n"), action,
					PATH_SHUTDOWNALLOW);
			return(EXIT_FAILURE);
		}

		/* Ok, lets get this box down */
		if (!strcmp(action, "poweroff")) {
			execl("/sbin/poweroff", "poweroff", NULL);
			execl("/sbin/halt", "halt", "-p", NULL);
			/* FALLTHROUGH */
		}

		if (!strcmp(action, "reboot")) {
			execl("/sbin/reboot", "reboot", NULL);
			/* FALLTHROUGH */
		}

		execl("/sbin/halt", "halt", NULL);

		fprintf(stderr, _(
			"Unable to shutdown this box. Please check your\n"
			"installation, or contact your system administrator\n"
			"and report the problem.\n"));
		return(EXIT_FAILURE);
	}

	/* try using sudo (should not return) */
	if (sudo(action)) {
		/*
		 * Ok, sudo returned indicating success, that means, the
		 * system is about to go down, so idle a bit
		 */
		for (;;)
			sleep(1);
	}

	fprintf(stderr, _(
		"Got no way to shutdown the system. You should ask\n"
		"your system administrator to either add your account\n"
		"name to %s or to install sudo(8) and grant\n"
		"you the right execute reboot and halt commands.\n"),
			PATH_SHUTDOWNALLOW);
	return(EXIT_FAILURE);
}
