/* $Id$ */
/*-
 * Copyright (c) 2003-2004 Benedikt Meurer <benny@xfce.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *                                                                              
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *                                                                              
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
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

/* settings */
static gboolean	autoSave = FALSE;
static gint chooserTimeout = 4;

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

	return MCS_PLUGIN_INIT_OK;
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
  xfce_rc_write_bool_entry (rc, "AutoSave", autoSave);
  xfce_rc_set_group (rc, "Chooser");
  xfce_rc_write_int_entry (rc, "Timeout", chooserTimeout);

  xfce_rc_close (rc);
}

/*
 */
static void
autoSaveChangedCB(GtkToggleButton *button, McsPlugin *plugin)
{
	autoSave = gtk_toggle_button_get_active (button);
	save_settings ();
}

static void
chooserTimeoutChangedCB (GtkSpinButton *spinner,
                         gpointer user_data)
{
  chooserTimeout = (gint) gtk_spin_button_get_value (spinner);
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
	GtkWidget *spinner;
	GtkWidget *header;
	GtkWidget *align;
	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *hbox;
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
  autoSave = xfce_rc_read_bool_entry (rc, "AutoSave", TRUE);
  xfce_rc_set_group (rc, "Chooser");
  chooserTimeout = xfce_rc_read_int_entry (rc, "Timeout", 4);
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
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), align, FALSE, TRUE, 0);

	/* General settings */
	frame = xfce_framebox_new(_("General"), TRUE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame, TRUE, TRUE, 0);

	vbox = gtk_vbox_new(FALSE, BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
	xfce_framebox_add(XFCE_FRAMEBOX(frame), vbox);

	/* */
	checkbox = gtk_check_button_new_with_label(
			_("Automatically save session on logout"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), autoSave);
	g_signal_connect(checkbox, "toggled", G_CALLBACK(autoSaveChangedCB),
			plugin);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, TRUE, 0);

 	/* Startup settings */
	frame = xfce_framebox_new(_("Startup"), TRUE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame, TRUE, TRUE, 0);

	vbox = gtk_vbox_new(FALSE, BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
	xfce_framebox_add(XFCE_FRAMEBOX(frame), vbox);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

  label = gtk_label_new (_("Chooser timeout in seconds:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

  spinner = gtk_spin_button_new_with_range (0.0, 180.0, 1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spinner), chooserTimeout);
  gtk_box_pack_start (GTK_BOX (hbox), spinner, FALSE, TRUE, 0);

  g_signal_connect (spinner, "value-changed",
                    G_CALLBACK (chooserTimeoutChangedCB), NULL);

	/* */
	gtk_widget_show_all(dialog);
}

/* */
MCS_PLUGIN_CHECK_INIT
