/* $Id$ */
/*-
 * Copyright (c) 2003,2004 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfcegui4/libxfcegui4.h>
#include <gtk/gtk.h>

#include "shutdown.h"

#define BORDER		6

/* whether to confirm logout */
gboolean	shutdownConfirm = TRUE;

/* whether to autosave session on logout */
gboolean	shutdownAutoSave = FALSE;

/* default action on logout */
gint		shutdownDefault = SHUTDOWN_LOGOUT;

/*
 * XXX - this is similar to the way gnome-session renders the background,
 * I'd like to get rid of this, and draw a stripes pixbuf instead, but I
 * don't know how, yet.
 */
static void
drawBackground(void)
{
	static char dash_list[] = {1, 1};
	GdkGCValues values;
	GdkGC *gc;
	int i;

	values.line_style = GDK_LINE_ON_OFF_DASH;
	values.subwindow_mode = GDK_INCLUDE_INFERIORS;

	gc = gdk_gc_new_with_values(gdk_get_default_root_window(),
			&values, GDK_GC_LINE_STYLE | GDK_GC_SUBWINDOW);
	gdk_gc_set_dashes(gc, 0, dash_list, 2);

#if 1
	for (i = 0; i < gdk_screen_width() / 2; i++) {
		int w = gdk_screen_width() - 2 * i;
		int h = gdk_screen_height() - 2 * i;

		if (h <= 0)
			h = 1;
		if (w <= 1)
			w = 1;

		gdk_draw_rectangle(gdk_get_default_root_window(), gc, FALSE,
				i, i, w, h);
	}
#else
	{
		GdkModifierType mmask;
		gint mx;
		gint my;

		gdk_window_get_pointer(gdk_get_default_root_window(), &mx, &my,
				&mmask);

	for (i = 0; i < MAX(gdk_screen_width(), gdk_screen_height()); i++) {
		int x = mx - i;
		int y = my - i;

		gdk_draw_rectangle(gdk_get_default_root_window(), gc, FALSE,
				x, y, i * 2, i * 2);
	}
	}
#endif

	gdk_flush();
	g_object_unref(gc);
}

/*
 */
static void
refreshBackground(void)
{
	GdkWindowAttr attr;
	GdkWindow *win;

	attr.x = 0;
	attr.y = 0;
	attr.window_type = GDK_WINDOW_TOPLEVEL;
	attr.wclass = GDK_INPUT_OUTPUT;
	attr.width = gdk_screen_width();
	attr.height = gdk_screen_height();
	attr.override_redirect = TRUE;
	attr.event_mask = 0;

	win = gdk_window_new(gdk_get_default_root_window(), &attr,
			GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR);

	gdk_window_show(win);
	gdk_flush();
	gdk_window_hide(win);
}

/*
 */
gboolean
shutdownDialog(gint *shutdownType, gboolean *saveSession)
{
	gboolean accessibility;
	const gchar *username;
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *dbox;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *image;
	GtkWidget *menu;
	GtkWidget *omenu;
	GtkWidget *mitem;
	GtkWidget *checkbox;
	GtkWidget *hidden;
	gint result;
	gchar *text;

	g_return_val_if_fail(saveSession != NULL, FALSE);
	g_return_val_if_fail(shutdownType != NULL, FALSE);

	/*
	 * The user do not want to see a "shutdown dialog", so lets assume,
	 * he's willing to execute the default (whatever that be)
	 */
	if (!shutdownConfirm) {
		*saveSession = shutdownAutoSave;
		*shutdownType = shutdownDefault;
		return(TRUE);
	}

	/* Try to grab Input on a hidden window first */
	hidden = gtk_invisible_new();
	gtk_widget_show(hidden);

	for (;;) {
		if (gdk_pointer_grab(hidden->window, FALSE, 0, NULL, NULL,
					GDK_CURRENT_TIME) == GDK_GRAB_SUCCESS) {
			if (gdk_keyboard_grab(hidden->window, FALSE,
						GDK_CURRENT_TIME)
					== GDK_GRAB_SUCCESS) {
			gtk_widget_destroy(hidden);
				break;
			}
			
			gdk_pointer_ungrab(GDK_CURRENT_TIME);
		}

		g_usleep(50 * 1000);
	}

	dialog = gtk_dialog_new_with_buttons(_("Logout session"),
			NULL, GTK_DIALOG_MODAL, 
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);

	/* set dialog window options */
	gtk_dialog_set_default_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

	/* this window *should* not be handled by the window manager */
	g_object_set(G_OBJECT(dialog), "type", GTK_WINDOW_POPUP, NULL);
#if GTK_CHECK_VERSION(2,3,0)
	g_object_set (G_OBJECT (dialog), "type_hint",
		      GDK_WINDOW_TYPE_HINT_UTILITY, NULL);
#else
	g_object_set (G_OBJECT (dialog), "decorated", FALSE, NULL);
#endif

	/*
	 * Grabbing the Xserver when accessibility is enabled will cause a
	 * hang. Found in gnome-session (see gnome bug #93103 for details).
	 */
	accessibility = GTK_IS_ACCESSIBLE(gtk_widget_get_accessible(dialog));
#if !GTK_CHECK_VERSION(2,3,0)
	if (!accessibility) {
		gdk_x11_grab_server();
		drawBackground();
		gdk_flush();
	}
#endif

	dbox = GTK_DIALOG(dialog)->vbox;

	/* if we cannot determine user's realname, fallback to account name */
	if ((username = g_get_real_name()) == NULL || !strlen(username))
		username = g_get_user_name();
	text = g_strdup_printf(_("End session for <b>%s</b>"), username);
	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), text);
	gtk_box_pack_start(GTK_BOX(dbox), label, FALSE, TRUE, BORDER);
	gtk_widget_show(label);
	g_free(text);

	/* */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(dbox), hbox, TRUE, TRUE, BORDER);
	gtk_widget_show(hbox);

	/* */
	image = gtk_image_new_from_stock(GTK_STOCK_QUIT, GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, TRUE, BORDER);
	gtk_widget_show(image);

	/* */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, BORDER);
	gtk_widget_show(vbox);

	/* */
	label = gtk_label_new(_("What do you want to do next?"));
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, BORDER);
	gtk_widget_show(label);

	/* */
	menu = gtk_menu_new();

	/* */
	mitem = gtk_menu_item_new_with_mnemonic(_("_Quit current session"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mitem);
	gtk_widget_show(mitem);

	/* */
	mitem = gtk_menu_item_new_with_mnemonic(_("_Reboot the computer"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mitem);
	gtk_widget_show(mitem);

	/* */
	mitem = gtk_menu_item_new_with_mnemonic(_("_Turn off computer"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mitem);
	gtk_widget_show(mitem);

	/* */
	omenu = gtk_option_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), shutdownDefault);
	gtk_box_pack_start(GTK_BOX(vbox), omenu, FALSE, TRUE, BORDER);
	gtk_widget_show(omenu);

	/* */
	checkbox = gtk_check_button_new_with_mnemonic(
			_("_Save session for future logins"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
			shutdownAutoSave);
	gtk_widget_set_sensitive(checkbox, !shutdownAutoSave);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, TRUE, BORDER);
	gtk_widget_show(checkbox);

	/* need to realize the dialog first! */
	gtk_widget_show_now(dialog);

	/* Grab Keyboard and Mouse pointer */
	gdk_pointer_grab(dialog->window, TRUE, 0, NULL, NULL, GDK_CURRENT_TIME);
	gdk_keyboard_grab(dialog->window, FALSE, GDK_CURRENT_TIME);
	XSetInputFocus(GDK_DISPLAY(), GDK_WINDOW_XWINDOW(dialog->window),
			RevertToParent, CurrentTime);

	/* run the logout dialog */
	result = gtk_dialog_run(GTK_DIALOG(dialog));

	if (result == GTK_RESPONSE_OK) {
		*saveSession = gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(checkbox));
		*shutdownType = gtk_option_menu_get_history(
				GTK_OPTION_MENU(omenu));
	}

	gtk_widget_destroy(dialog);

#if !GTK_CHECK_VERSION(2, 3, 0)
	/* ungrab the Xserver */
	if (!accessibility) {
		gdk_x11_ungrab_server();
		refreshBackground();
	}
#endif

	/* Release Keyboard/Mouse pointer grab */
	gdk_keyboard_ungrab(GDK_CURRENT_TIME);
	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gdk_flush();

	/*
	 * run an iteration through the main loop to give the
	 * confirm dialog time to disappear
	 */
	g_main_context_iteration(NULL, TRUE);

	return(result == GTK_RESPONSE_OK);
}

/*
 */
gint
shutdown(gint type)
{
	gchar *command;
	gchar *fallback;
	gchar *buf;
	GError *error;
	gint status;

	error = NULL;
  fallback = NULL;

	switch (type) {
	case SHUTDOWN_REBOOT:
		command = g_strdup_printf("%s reboot", SHUTDOWN_COMMAND);
		break;

	case SHUTDOWN_HALT:
		command = g_strdup_printf("%s poweroff", SHUTDOWN_COMMAND);
    fallback = g_strdup_printf("%s halt", SHUTDOWN_COMMAND);
		break;

	default:
		return(EXIT_SUCCESS);
	}

again:
	if (!g_spawn_command_line_sync(command, NULL, &buf, &status, &error)) {
    if (fallback != NULL)
      goto try_fallback;

		xfce_err(_("The following error occured while trying to "
			   "shutdown the computer:\n\n%s"), error->message);
		g_error_free(error);
		g_free(command);
		return(EXIT_FAILURE);
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    if (fallback != NULL)
      goto try_fallback;

		xfce_err(_(
			"The following error occured while trying to "
			"shutdown the computer:\n\n%s"), buf);
		g_free(command);
		return(EXIT_FAILURE);
	}

	g_free(command);
	return(EXIT_SUCCESS);

try_fallback:
  // try using fallback command
  g_free(command);
  command = fallback;
  fallback = NULL;
  goto again;
}

