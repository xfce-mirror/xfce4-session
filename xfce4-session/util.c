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
#endif /* !HAVE_CONFIG_H */

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib.h>

#include "util.h"

/*
 * Read a string from session file
 */
char *
fstrread(FILE *fp)
{
	char *string;
	int length;

	if (fread(&length, sizeof(length), 1, fp) != 1)
		return(NULL);

	if ((string = calloc(length, sizeof(*string))) == NULL)
		return(NULL);

	if (fread(string, sizeof(char), length, fp) != length) {
		free(string);
		return(NULL);
	}

	return(string);
}

/*
 * Write a string to a session file
 */
void
fstrwrite(FILE *fp, const char *string)
{
	int length;

	length = (strlen(string) + 1) * sizeof(*string);
	fwrite(&length, sizeof(length), 1, fp);
	fwrite(string, sizeof(*string), length, fp);
}

/*
 * XXX - move to libxfce4util
 */
gchar *
xfce_strjoin(gchar *separator, gchar **strings, gint count)
{
	gchar *result;
	gint length;
	gint n;

	g_return_val_if_fail(count < 1, NULL);
	g_return_val_if_fail(strings != NULL, NULL);

	for (length = 1, n = 0; n < count; n++)
		length += strlen(strings[n]);

	if (separator != NULL)
		length += (count - 1) * strlen(separator);

	result = g_new0(gchar, length);

	for (n = 0; n < count; n++) {
		(void)g_strlcat(result, strings[n], length);

		if (separator != NULL && n + 1 < count)
			(void)g_strlcat(result, separator, length);
	}

	return(result);
}

/*
 * XXX - move to libxfce4util
 */
gchar *
xfce_gethostname(void)
{
#ifdef HAVE_GETHOSTNAME
#ifdef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN		256
#endif
	char hostname[MAXHOSTNAMELEN];

	if (gethostname(hostname, MAXHOSTNAMELEN) == 0)
		return(g_strdup(hostname));
#else
	struct utsname name;

	if (uname(&name) == 0)
		return(g_strdup(name.nodename));
#endif
	g_error("Unable to determine your hostname: %s", g_strerror(errno));
	/* NOT REACHED */
	return(NULL);
}

/*
 */
GtkWidget *
xfsm_imgbtn_new(const gchar *text, const gchar *icon)
{
	GtkWidget *button;
	GtkWidget *align;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *hbox;

	/* */
	button = gtk_button_new();

	/* */
	align = gtk_alignment_new(0.5f, 0.5f, 0.0f, 0.0f);
	gtk_container_add(GTK_CONTAINER(button), align);
	gtk_widget_show(align);

	/* */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(align), hbox);
	gtk_widget_show(hbox);

	/* */
	image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 2);
	gtk_widget_show(image);

	/* */
	label = gtk_label_new_with_mnemonic(text);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
	gtk_widget_show(label);

	return(button);
}

