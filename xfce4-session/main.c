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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <libxfce4mcs/mcs-client.h>
#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <xfce4-session/ice-layer.h>
#include <xfce4-session/manager.h>
#include <xfce4-session/shutdown.h>
#include <xfce4-session/splash-screen.h>
#include <xfce4-session/tray-icon.h>

/* */
#define	CHANNEL	"session"

/* */
McsClient	*settingsClient;

/*
 */
static gboolean
ready_default_session(GtkWidget *splash)
{
	gtk_widget_destroy(splash);
	return(FALSE);
}

/*
 */
static gboolean
cont_default_session(XfsmSplashScreen *splash)
{
	xfsm_splash_screen_launch(splash, _("desktop"));
	g_timeout_add(3 * 1000, (GSourceFunc)ready_default_session, splash);
	return(FALSE);
}

/*
 * Start a default XFce4 session
 */
static gboolean
start_default_session(void)
{
	extern gchar *startupSplashTheme;
	GtkWidget *splash;
	GError *error;

	/* try to launch the default session script */
	if (!g_spawn_command_line_async("/bin/sh " DEFAULT_SESSION, &error)) {
		xfce_err("The session manager was unable to start the\n"
			 "default session. This is most often caused\n"
			 "by a broken installation of the session manager.\n"
			 "Please contact your local system administrator\n"
			 "and report the problem.");
		g_idle_add((GSourceFunc)exit, GUINT_TO_POINTER(EXIT_FAILURE));
	}
	else {
		/* show up splash screen */
		splash = xfsm_splash_screen_new(startupSplashTheme,
				1, _("Starting session manager.."));
		gtk_widget_show(splash);
		g_idle_add((GSourceFunc)cont_default_session, splash);
	}

	return(FALSE);
}

/*
 * Run a sanity check before logging in
 */
static gboolean
sanity_check(gchar **message)
{
	gchar *path;

	path = (gchar *)xfce_get_userdir();

	if (!g_file_test(path, G_FILE_TEST_IS_DIR) && mkdir(path, 0755) < 0) {
		*message = g_strdup_printf(_(
				"Unable to create users XFce settings\n"
				"directory %s: %s"), path, g_strerror(errno));
		return(FALSE);
	}

	path = xfce_get_userfile("sessions", NULL);

	if (!g_file_test(path, G_FILE_TEST_IS_DIR) && mkdir(path, 0755) < 0) {
		*message = g_strdup_printf(_(
				"Unable to create users XFce session\n"
				"directory %s: %s"), path, g_strerror(errno));
		return(FALSE);
	}

	g_free(path);

	return(TRUE);
}

/*
 */
static void
settings_notify(const char *name, const char *channel, McsAction action,
		McsSetting *setting, void *data)
{
	/* XXX */
	extern gboolean	shutdownConfirm;
	extern gboolean	shutdownAutoSave;
	extern gint	shutdownDefault;
	extern gchar	*startupSplashTheme;

	switch (action) {
	case MCS_ACTION_NEW:
	case MCS_ACTION_CHANGED:
		if (setting->type == MCS_TYPE_INT) {
			if (!strcmp(name, "Session/ConfirmLogout"))
				shutdownConfirm = setting->data.v_int;
			else if (!strcmp(name, "Session/AutoSave"))
				shutdownAutoSave = setting->data.v_int;
			else if (!strcmp(name, "Session/DefaultAction"))
				shutdownDefault = setting->data.v_int;
		}
		else if (setting->type == MCS_TYPE_STRING) {
			if (!strcmp(name, "Session/StartupSplashTheme")) {
				if (startupSplashTheme != NULL)
					g_free(startupSplashTheme);
				startupSplashTheme =
					g_strdup(setting->data.v_string);
			}
		}
		break;
		
	case MCS_ACTION_DELETED:
	default:
		break;
	}
}

/*
 */
static GdkFilterReturn
settings_filter(GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	if (mcs_client_process_event(settingsClient, (XEvent *)xevent))
		return(GDK_FILTER_REMOVE);
	else
		return(GDK_FILTER_CONTINUE);
}

/*
 */
static void
settings_watch(Window xwindow, Bool starting, long mask, void *data)
{
	GdkWindow *window;

	window = gdk_window_lookup(xwindow);

	if (starting) {
		if (window == NULL)
			window = gdk_window_foreign_new(xwindow);
		else
			g_object_ref(window);
		gdk_window_add_filter(window, settings_filter, data);
	}
	else {
		gdk_window_remove_filter(window, settings_filter, data);
		g_object_unref(window);
	}
}

/*
 */
static void
toggle_visible(GtkWidget *widget)
{
	if (GTK_WIDGET_VISIBLE(widget))
		gtk_widget_hide(widget);
	else
		gtk_widget_show(widget);
}

/*
 */
static void
show_preferences(void)
{
	(void)g_spawn_command_line_async("xfce-setting-show session", NULL);
}

/*
 */
static GtkWidget *
create_tray_icon(void)
{
	/* XXX */
	extern GtkWidget *clientList;
	GtkWidget *defaultItem;
	GtkWidget *menuItem;
	GtkWidget *menu;
	GtkWidget *icon;

	menu = gtk_menu_new();

	/* */
	menuItem = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES,
			NULL);
	g_signal_connect(menuItem, "activate", 
			G_CALLBACK(show_preferences), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuItem);
	gtk_widget_show(menuItem);

	/* */
	defaultItem = gtk_menu_item_new_with_mnemonic(_("Session control"));
	g_signal_connect_swapped(defaultItem, "activate",
			G_CALLBACK(toggle_visible), clientList);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), defaultItem);
	gtk_widget_show(defaultItem);

	icon = xfsm_tray_icon_new(GTK_MENU(menu), defaultItem);

	return(icon);
}

/*
 */
int
main(int argc, char **argv)
{
	extern gchar *startupSplashTheme;
	McsSetting *setting;
	gchar *message;
	
	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	gtk_init(&argc, &argv);

	/* run a sanity check before we start the actual session manager */
	if (!sanity_check(&message)) {
		xfce_err("%s", message);
		return(EXIT_FAILURE);
	}

	if (!manager_init())
		g_error("Unable to initialize session manager");

	/* make sure the MCS manager is running */
	if (!mcs_client_check_manager(GDK_DISPLAY(),
				DefaultScreen(GDK_DISPLAY()),
				"xfce-mcs-manager")) {
		xfce_err(_(
			"The session manager was unable to start the\n"
			"Multi-Channel settings manager. This is most\n"
			"often caused by a broken XFce installation.\n"
			"Please contact your local system administrator\n"
			"and report the problem."));
		return(EXIT_FAILURE);
	}

	/* connect to the settings manager */
	if ((settingsClient = mcs_client_new(GDK_DISPLAY(),
				DefaultScreen(GDK_DISPLAY()),
				settings_notify, settings_watch,
				NULL)) == NULL) {
		g_error(_("Unable to create MCS client"));
	}
	else
		mcs_client_add_channel(settingsClient, CHANNEL);

	/* query MCS splash theme setting */
	if (startupSplashTheme != NULL) {
		g_free(startupSplashTheme);
		startupSplashTheme = NULL;
	}
	
	if (mcs_client_get_setting(settingsClient,
				"Session/StartupSplashTheme",
				CHANNEL, &setting) == MCS_SUCCESS) {
		startupSplashTheme = g_strdup(setting->data.v_string);
		g_free(setting);
	}
	else {
		const gchar *theme;
		if ((theme = g_getenv("XFSM_SPLASH_THEME")) != NULL)
			startupSplashTheme = g_strdup(theme);
		g_warning("Failed to get splash theme setting");
	}

	/*
	 * the manager was unable to restart a previous session, so we
	 * simply start a new default session.
	 */
	if (!manager_restart())
		g_idle_add((GSourceFunc)start_default_session, NULL);

	/*
	 */
	gtk_widget_show(create_tray_icon());

	gtk_main();

	if (shutdownSave && !manager_save())
		g_warning("Unable to save session");

	return(shutdown(shutdownType));
}

