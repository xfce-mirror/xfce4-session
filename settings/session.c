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

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4mcs/mcs-manager.h>
#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <xfce-mcs-manager/manager-plugin.h>

#include <xfce4-session/shutdown.h>

#include "session-icon.h"

/* */
#define	RCDIR		"settings"
#define CHANNEL		"session"
#define	RCFILE		"session.xml"

/* */
#define BORDER		6

/* max length of a theme name */
#define	MAXTHEMELEN	128

/* */
#define	MAXINFOLEN	512

/* prototypes */
static void	run_dialog(McsPlugin *);
static gboolean	save_settings(McsPlugin *);
static void	confirmLogoutChangedCB(GtkToggleButton *, McsPlugin *);
static void	autoSaveChangedCB(GtkToggleButton *, McsPlugin *);
static void	defaultActionChangedCB(GtkOptionMenu *, McsPlugin *);

/* settings */
static gboolean	confirmLogout = TRUE;
static gboolean	autoSave = FALSE;
static gint	defaultAction = SHUTDOWN_LOGOUT;

/* */
static GtkWidget	 *dialog = NULL;

/* themes option menu */
static GtkWidget	 *themesMenu = NULL;

/* */
static GtkTooltips	*tooltips = NULL;

/* */
typedef struct {
	gchar	name[MAXTHEMELEN];
	gchar	title[MAXTHEMELEN];
	gchar	preview[PATH_MAX];
	gchar	info[MAXINFOLEN];
	gchar	author[MAXINFOLEN];
} SplashTheme;

#define MAX_THEMES		25

/* list of installed splash themes */
static gint		themeCount = 0;
static gint		themeCurrent = 0;
static SplashTheme	themes[MAX_THEMES];

/*
 */
static gboolean
read_theme(const gchar *file, SplashTheme *theme)
{
	gchar buffer[LINE_MAX];
	gchar *dir;
	gchar *p;
	gchar *s;
	FILE *fp;

	if ((fp = fopen(file, "r")) == NULL)
		return(FALSE);

	if (fgets(buffer, LINE_MAX, fp) == NULL ||
	    strncmp(buffer, "[Splash Theme]", 14) != 0)
		goto failed;

	/* init theme info */
	memset(theme, 0, sizeof(*theme));

	while (fgets(buffer, LINE_MAX, fp) != NULL) {
		/* strip leading and trailing whitespace */
		p = g_strstrip(buffer);

		if (strncmp(p, "name=", 5) == 0 && strlen(p + 5) > 0)
			g_strlcpy(theme->title, p + 5, MAXTHEMELEN);
		else if (strncmp(p, "info=", 5) == 0 && strlen(p + 5) > 0) {
			s = g_strcompress(p + 5);
			g_strlcpy(theme->info, s, MAXINFOLEN);
			g_free(s);
		}
		else if (strncmp(p, "author=", 7) == 0 && strlen(p + 7) > 0) {
			s = g_strcompress(p + 7);
			g_strlcpy(theme->author, s, MAXINFOLEN);
			g_free(s);
		}
		else if (strncmp(p, "preview=", 8) == 0 && strlen(p + 8) > 0) {
			dir = g_path_get_dirname(file);
			g_snprintf(theme->preview, PATH_MAX, "%s%s%s", dir,
					G_DIR_SEPARATOR_S, p + 8);
			g_free(dir);
		}
	}

	/* check if a name was given */
	if (strlen(theme->title) == 0)
		goto failed;

	(void)fclose(fp);
	return(TRUE);

failed:
	(void)fclose(fp);
	return(FALSE);
}

/*
 */
static void
find_themes(McsPlugin *plugin)
{
	McsSetting *setting;
	const gchar *entry;
	gchar *file;
	gchar *dir;
	GDir *dp;
	gint n;

	themeCount = 0;
	themeCurrent = 0;

	/* find themes in users ~/.xfce4/splash/ dir */
	dir = xfce_get_userfile("splash", NULL);
	if ((dp = g_dir_open(dir, 0, NULL)) != NULL) {
		while ((entry = g_dir_read_name(dp)) && themeCount<MAX_THEMES) {
			file = g_build_filename(dir, entry,"splash.theme",NULL);

			if (read_theme(file, themes + themeCount)) {
				g_strlcpy(themes[themeCount++].name, entry,
						MAXTHEMELEN);
			}

			g_free(file);
		}
	}
	g_free(dir);

	/* find system wide splash themes */
	dir = SPLASH_THEMES_DIR;
	if ((dp = g_dir_open(dir, 0, NULL)) != NULL) {
		while ((entry = g_dir_read_name(dp)) && themeCount<MAX_THEMES) {
			file = g_build_filename(dir, entry,"splash.theme",NULL);

			/* check if theme is already listed */
			for (n = 0; n < themeCount; n++)
				    if (!strcmp(themes[n].name, entry))
					break;

			/* It was already listed */
			if (n < themeCount) {
				g_free(file);
				continue;
			}

			if (read_theme(file, themes + themeCount)) {
				g_strlcpy(themes[themeCount++].name, entry,
						MAXTHEMELEN);
			}

			g_free(file);
		}
	}

	/* */
	if (themeCount == 0) {
		/* XXX - add "pseudo" Default theme */
		g_strlcpy(themes->name, "Default", MAXTHEMELEN);
		g_strlcpy(themes->title, _("Default Theme"), MAXTHEMELEN);
		g_strlcpy(themes->author,
				"Benedikt Meurer\n"
				"<benedikt.meurer@unix-ag.org>",
				MAXINFOLEN);
		g_strlcpy(themes->info, _("Default splash screen"), MAXINFOLEN);
		themes->preview[0] = '\0';
		themeCount = 1;
	}
	
	/* update MCS setting */
	if ((setting = mcs_manager_setting_lookup(plugin->manager,
					"Session/StartupSplashTheme",
					CHANNEL)) != NULL) {
		for (n = 0; n < themeCount; n++) {
			if (!strcmp(themes[n].name, setting->data.v_string)) {
				themeCurrent = n;
				break;
			}
		}
	}
	else {
		mcs_manager_set_string(plugin->manager,
				"Session/StartupSplashTheme",
				CHANNEL, themes[themeCurrent].name);
	}

}

/*
 */
static void
show_info_dialog(void)
{
	SplashTheme *theme;
	gchar title[256];
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *button;

	theme = themes + themeCurrent;
	g_snprintf(title, 256, _("About %s..."), theme->title);

	dialog = gtk_dialog_new_with_buttons(title, NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR,
			NULL);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

	/* */
	gtk_button_box_set_layout(
			GTK_BUTTON_BOX(GTK_DIALOG(dialog)->action_area),
			GTK_BUTTONBOX_EDGE);

	/* */
	vbox = GTK_DIALOG(dialog)->vbox;

	/* */
	image = gtk_image_new_from_file(theme->preview);
	gtk_box_pack_start(GTK_BOX(vbox), image, TRUE, TRUE, BORDER);

	/* */
	hbox = gtk_hbox_new(FALSE, BORDER);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, BORDER);
	label = gtk_label_new(_("Info:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, BORDER);
	label = gtk_label_new(theme->info);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, BORDER);

	/* */
	hbox = gtk_hbox_new(FALSE, BORDER);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, BORDER);
	label = gtk_label_new(_("Author:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, BORDER);
	label = gtk_label_new(theme->author);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, BORDER);

	/* */
	button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	g_signal_connect_swapped(button, "clicked",
			G_CALLBACK(gtk_widget_destroy),
			dialog);
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button,
			GTK_RESPONSE_OK);

	/* */
	gtk_widget_show_all(dialog);

	/* */
	g_signal_connect_swapped(dialog, "response",
			G_CALLBACK(gtk_widget_destroy),
			dialog);
	g_signal_connect_swapped(dialog, "delete-event",
			G_CALLBACK(gtk_widget_destroy),
			dialog);
}

/*
 */
static void
rebuild_themes_menu(void)
{
	GtkWidget *menu;
	GtkWidget *item;
	gint n;

	gtk_widget_destroy(gtk_option_menu_get_menu(
				GTK_OPTION_MENU(themesMenu)));

	menu = gtk_menu_new();

	/* */
	for (n = 0; n < themeCount; n++) {
		item = gtk_menu_item_new_with_label(themes[n].title);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(themesMenu), menu);
}

/*
 * XXX - "tar" execution could be done better :-)
 */
static void
do_install_theme(GtkWidget *dialog, gpointer data)
{
	gchar * argv[] = { "tar", "xzf", NULL, NULL };
	const gchar *filename;
	GError *error;
	gchar *dir;
	
	filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(dialog));
	argv[2] = (gchar *)filename;
	dir = xfce_get_userfile("splash", NULL);
	error = NULL;

	/* check if users splash themes directory exists */
	if (!g_file_test(dir, G_FILE_TEST_IS_DIR))
		(void)mkdir(dir, 0755);

	if (!g_spawn_sync(dir, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
				NULL, NULL, NULL, &error)) {
		xfce_err(_("Unable to install splash theme from file %s: %s"),
				filename, error->message);
		g_error_free(error);
	}
	else {
		/* rescan themes */
		find_themes((McsPlugin *)g_object_get_data(G_OBJECT(dialog),
					"user-data"));
		rebuild_themes_menu();
	}
	
	g_free(dir);
}

/*
 */
static void
install_theme(GtkWidget *button, McsPlugin *plugin)
{
	GtkWidget *dialog;

	dialog = gtk_file_selection_new(_("Install new theme"));
	gtk_file_selection_complete(GTK_FILE_SELECTION(dialog), "*.tar.gz");
	g_object_set_data(G_OBJECT(dialog), "user-data", plugin);

	g_signal_connect_swapped(GTK_FILE_SELECTION(dialog)->ok_button,
			"clicked", G_CALLBACK(do_install_theme), dialog);

	/*
	 * Ensure that the dialog box is destroyed when the user clicks a
	 * button.
	 */
	g_signal_connect_swapped(GTK_FILE_SELECTION(dialog)->ok_button,
			"clicked", G_CALLBACK(gtk_widget_destroy), dialog);

	g_signal_connect_swapped(GTK_FILE_SELECTION(dialog)->cancel_button,
			"clicked", G_CALLBACK(gtk_widget_destroy), dialog);
   
	/* Display that dialog */
	gtk_widget_show(dialog);
}

/*
 */
McsPluginInitResult
mcs_plugin_init(McsPlugin *plugin)
{
	McsSetting *setting;
	gchar *file;

	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	file = xfce_get_userfile(RCDIR, RCFILE, NULL);
	mcs_manager_add_channel_from_file(plugin->manager, CHANNEL, file);
	g_free(file);

	/* search installed splash themes */
	find_themes(plugin);

	if ((setting = mcs_manager_setting_lookup(plugin->manager,
					"Session/ConfirmLogout",
					CHANNEL)) != NULL) {
		confirmLogout = setting->data.v_int;
	}
	else {
		mcs_manager_set_int(plugin->manager, "Session/ConfirmLogout",
				CHANNEL, confirmLogout);
	}

	if ((setting = mcs_manager_setting_lookup(plugin->manager,
					"Session/AutoSave",
					CHANNEL)) != NULL) {
		autoSave = setting->data.v_int;
	}
	else {
		mcs_manager_set_int(plugin->manager, "Session/AutoSave",
				CHANNEL, autoSave);
	}

	if ((setting = mcs_manager_setting_lookup(plugin->manager,
					"Session/DefaultAction",
					CHANNEL)) != NULL) {
		defaultAction = setting->data.v_int;
	}
	else {
		mcs_manager_set_int(plugin->manager, "Session/DefaultAction",
				CHANNEL, defaultAction);
	}

	plugin->plugin_name = g_strdup("session");
	plugin->caption = g_strdup(_("Session management"));
	plugin->run_dialog = run_dialog;
	plugin->icon = inline_icon_at_size(session_icon_data, 48, 48);

	return(MCS_PLUGIN_INIT_OK);
}

/*
 */
static gboolean
save_settings(McsPlugin *plugin)
{
	gboolean result;
	gchar *file;

	file = xfce_get_userfile(RCDIR, RCFILE, NULL);
	result = mcs_manager_save_channel_to_file(plugin->manager, CHANNEL,
			file);
	g_free(file);

	return(result);
}

/*
 */
static void
confirmLogoutChangedCB(GtkToggleButton *button, McsPlugin *plugin)
{
	confirmLogout = gtk_toggle_button_get_active(button);
	mcs_manager_set_int(plugin->manager, "Session/ConfirmLogout",
			CHANNEL, confirmLogout);
	mcs_manager_notify(plugin->manager, CHANNEL);
	save_settings(plugin);
}

/*
 */
static void
autoSaveChangedCB(GtkToggleButton *button, McsPlugin *plugin)
{
	autoSave = gtk_toggle_button_get_active(button);
	mcs_manager_set_int(plugin->manager, "Session/AutoSave",
			CHANNEL, autoSave);
	mcs_manager_notify(plugin->manager, CHANNEL);
	save_settings(plugin);
}

/*
 */
static void
defaultActionChangedCB(GtkOptionMenu *omenu, McsPlugin *plugin)
{
	defaultAction = gtk_option_menu_get_history(omenu);
	mcs_manager_set_int(plugin->manager, "Session/DefaultAction",
			CHANNEL, defaultAction);
	mcs_manager_notify(plugin->manager, CHANNEL);
	save_settings(plugin);
}

/*
 */
static void
splashThemeChangedCB(GtkOptionMenu *omenu, McsPlugin *plugin)
{
	themeCurrent = gtk_option_menu_get_history(omenu);
	mcs_manager_set_string(plugin->manager, "Session/StartupSplashTheme",
			CHANNEL, themes[themeCurrent].name);
	mcs_manager_notify(plugin->manager, CHANNEL);
	save_settings(plugin);
}

/*
 */
static void
responseCB(McsPlugin *plugin)
{
	save_settings(plugin);
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
	GtkWidget *hbox;
	GtkWidget *menu;
	GtkWidget *omenu;
	GtkWidget *mitem;
	GtkWidget *button;
	GtkWidget *image;
	gint n;

	if (dialog != NULL) {
		gtk_window_present(GTK_WINDOW(dialog));
		return;
	}

	/* search installed splash themes */
	find_themes(plugin);

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

	header = create_header(plugin->icon, _("Session management"));
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
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
			confirmLogout);
	g_signal_connect(checkbox, "toggled",G_CALLBACK(confirmLogoutChangedCB),
			plugin);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, TRUE, 0);

	/* */
	checkbox = gtk_check_button_new_with_label(
			_("Automatically save session on logout"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
			autoSave);
	g_signal_connect(checkbox, "toggled", G_CALLBACK(autoSaveChangedCB),
			plugin);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, TRUE, 0);

	/* Logout action settings */
	frame = xfce_framebox_new(_("Default action on logout"), TRUE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame, TRUE,
			TRUE, 0);

	vbox = gtk_vbox_new(FALSE, BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
	xfce_framebox_add(XFCE_FRAMEBOX(frame), vbox);

	/* */
	menu = gtk_menu_new();

	/* */
	mitem = gtk_menu_item_new_with_mnemonic(_("_Quit current session"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mitem);

	/* */
	mitem = gtk_menu_item_new_with_mnemonic(_("_Reboot the computer"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mitem);

	/* */
	mitem = gtk_menu_item_new_with_mnemonic(_("_Turn off computer"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mitem);

	/* */
	omenu = gtk_option_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(omenu), defaultAction);
	g_signal_connect(omenu, "changed", G_CALLBACK(defaultActionChangedCB),
			plugin);
	gtk_box_pack_start(GTK_BOX(vbox), omenu, FALSE, TRUE, BORDER);

	/* Logout action settings */
	frame = xfce_framebox_new(_("Splash screen theme"), TRUE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame, TRUE,
			TRUE, 0);

	vbox = gtk_vbox_new(FALSE, BORDER);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
	xfce_framebox_add(XFCE_FRAMEBOX(frame), vbox);

	/* */
	menu = gtk_menu_new();

	/* */
	for (n = 0; n < themeCount; n++) {
		mitem = gtk_menu_item_new_with_label(themes[n].title);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), mitem);
	}

	/* */
	hbox = gtk_hbox_new(FALSE, BORDER);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, BORDER);

	/* */
	themesMenu = gtk_option_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(themesMenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(themesMenu), themeCurrent);
	g_signal_connect(themesMenu,"changed", G_CALLBACK(splashThemeChangedCB),
			plugin);
	gtk_box_pack_start(GTK_BOX(hbox), themesMenu, TRUE, TRUE, 0);

	/* Info button */
	button = gtk_button_new();
	gtk_tooltips_set_tip(tooltips, button, _("Show theme info"), NULL);
	image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO,
			GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(button), image);
	g_signal_connect(button, "clicked", G_CALLBACK(show_info_dialog), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);

	/* Install button */
	button = gtk_button_new();
	gtk_tooltips_set_tip(tooltips, button, _("Install new theme"), NULL);
	image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(button), image);
	g_signal_connect(button, "clicked", G_CALLBACK(install_theme), plugin);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);

	/* */
	gtk_widget_show_all(dialog);
}

/* */
MCS_PLUGIN_CHECK_INIT
