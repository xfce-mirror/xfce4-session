/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
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

#include <libxfce4mcs/mcs-manager.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <xfce-mcs-manager/manager-plugin.h>

#include <xfce4-session/shutdown.h>

#include "session-icon.h"

/* */
#define BORDER		6

/* max length of a theme name */
#define	MAXTHEMELEN	128

/* */
#define	MAXINFOLEN	512

/* prototypes */
static void	run_dialog(McsPlugin *);
static void	save_settings(void);
static void	confirmLogoutChangedCB(GtkToggleButton *, McsPlugin *);
static void	autoSaveChangedCB(GtkToggleButton *, McsPlugin *);

/* settings */
static gboolean	confirmLogout = TRUE;
static gboolean	autoSave = FALSE;
static gboolean	displayChooser = FALSE;

/* */
static GtkWidget	 *dialog = NULL;

/* */
static GtkTooltips	*tooltips = NULL;

/*
 */
McsPluginInitResult
mcs_plugin_init(McsPlugin *plugin)
{
	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	plugin->plugin_name = g_strdup("session");
	plugin->caption = g_strdup(_("Session management"));
	plugin->run_dialog = run_dialog;
	plugin->icon = xfce_inline_icon_at_size(session_icon_data, 48, 48);

	return(MCS_PLUGIN_INIT_OK);
}

/*
 */
static void
save_settings(void)
{
  XfceRc *rc;

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG,
                            "xfce4-session/xfce4-session.rc",
                            FALSE);

  xfce_rc_set_group (rc, "General");
  xfce_rc_write_bool_entry (rc, "ConfirmLogout", confirmLogout);
  xfce_rc_write_bool_entry (rc, "AutoSave", autoSave);
  xfce_rc_write_bool_entry (rc, "AlwaysDisplayChooser", displayChooser);

  xfce_rc_close (rc);
}

/*
 */
static void
confirmLogoutChangedCB(GtkToggleButton *button, McsPlugin *plugin)
{
	confirmLogout = gtk_toggle_button_get_active(button);
	save_settings();
}

/*
 */
static void
autoSaveChangedCB(GtkToggleButton *button, McsPlugin *plugin)
{
	autoSave = gtk_toggle_button_get_active(button);
	save_settings();
}

static void
displayChooserChangedCB(GtkToggleButton *button, McsPlugin *plugin)
{
  displayChooser = gtk_toggle_button_get_active (button);
  save_settings ();
}

/*
 */
static void
responseCB(McsPlugin *plugin)
{
	save_settings();
	gtk_widget_destroy(dialog);
	dialog = NULL;
}

/*
 */
static void
run_dialog(McsPlugin *plugin)
{
	GtkWidget *checkbox;
	GtkWidget *header;
	GtkWidget *align;
	GtkWidget *frame;
	GtkWidget *vbox;
  XfceRc *rc;

	if (dialog != NULL) {
		gtk_window_present(GTK_WINDOW(dialog));
		return;
	}

	/* load settings */
  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG,
                            "xfce4-session/xfce4-session.rc",
                            TRUE);
  xfce_rc_set_group (rc, "General");
  confirmLogout = xfce_rc_read_bool_entry (rc, "ConfirmLogout", TRUE);
  autoSave = xfce_rc_read_bool_entry (rc, "AutoSave", TRUE);
  displayChooser = xfce_rc_read_bool_entry (rc, "AlwaysDisplayChooser", FALSE);
  xfce_rc_close (rc);

	/* */
	if (tooltips == NULL)
		tooltips = gtk_tooltips_new();

	dialog = gtk_dialog_new_with_buttons(_("Session management"), NULL,
			GTK_DIALOG_NO_SEPARATOR,
			GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
			NULL);

	gtk_window_set_icon(GTK_WINDOW(dialog), plugin->icon);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

	g_signal_connect_swapped(dialog, "response", G_CALLBACK(responseCB), 
			plugin);
	g_signal_connect_swapped(dialog, "delete-event",G_CALLBACK(responseCB),
			plugin);

	header = xfce_create_header(plugin->icon, _("Session management"));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), header, FALSE,
			TRUE, 0);

	/* */
	align = gtk_alignment_new(0, 0, 0, 0);
	gtk_widget_set_size_request(align, BORDER, BORDER);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), align, FALSE,
			TRUE, 0);

	/* General settings */
	frame = xfce_framebox_new(_("General"), TRUE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame, TRUE,
			TRUE, 0);

	vbox = gtk_vbox_new(FALSE, BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
	xfce_framebox_add(XFCE_FRAMEBOX(frame), vbox);

	/* */
	checkbox = gtk_check_button_new_with_label(_("Confirm logout"));
	gtk_tooltips_set_tip(tooltips, checkbox, _(
			"Should the session manager ask the user to confirm "
			"the logout. If disabled, the session will be closed "
			"without any further user interaction."), NULL);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
			confirmLogout);
	g_signal_connect(checkbox, "toggled",G_CALLBACK(confirmLogoutChangedCB),
			plugin);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, TRUE, 0);

	/* */
	checkbox = gtk_check_button_new_with_label(
			_("Automatically save session on logout"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), autoSave);
	g_signal_connect(checkbox, "toggled", G_CALLBACK(autoSaveChangedCB),
			plugin);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, TRUE, 0);

  checkbox = gtk_check_button_new_with_label(
      _("Display session chooser on each login"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), displayChooser);
  g_signal_connect (checkbox, "toggled", G_CALLBACK (displayChooserChangedCB),
      plugin);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, TRUE, 0);

	/* */
	gtk_widget_show_all(dialog);
}

/* */
MCS_PLUGIN_CHECK_INIT
