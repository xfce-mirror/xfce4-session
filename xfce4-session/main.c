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

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <libxfce4mcs/mcs-client.h>
#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <settings/session-icon.h>
#include <xfce4-session/ice-layer.h>
#include <xfce4-session/manager.h>
#include <xfce4-session/shutdown.h>
#include <xfce4-session/splash-screen.h>
#include <xfce4-session/xfce_trayicon.h>

/* */
#define	CHANNEL	"session"

/* UNIX signal states */
enum
{
	SIGNAL_NONE,
	SIGNAL_SAVE,
	SIGNAL_QUIT
};

/* current UNIX signal state */
static gint	signalState = SIGNAL_NONE;

/* */
McsClient	*settingsClient;

/* system tray icon */
XfceTrayIcon	*trayIcon;

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
			else if (!strcmp(name, "Session/TrayIcon")) {
				if ((gboolean)setting->data.v_int)
					xfce_tray_icon_connect(trayIcon);
				else
					xfce_tray_icon_disconnect(trayIcon);
			}
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
toggle_visible_cb(GtkWidget *widget)
{
	if (GTK_WIDGET_VISIBLE(widget))
		gtk_widget_hide(widget);
	else
		gtk_widget_show(widget);
}

/*
 */
static void
show_preferences_cb(void)
{
	(void)g_spawn_command_line_async("xfce-setting-show session", NULL);
}

/*
 */
static void
save_session_cb(void)
{
	manager_saveyourself(SmSaveBoth, False, SmInteractStyleNone, False);
}

/*
 */
static void
quit_session_cb(void)
{
	manager_saveyourself(SmSaveBoth, True, SmInteractStyleAny, False);
}

/*
 */
static XfceTrayIcon *
create_tray_icon(void)
{
	/* XXX */
	extern GtkWidget *sessionControl;
	GtkWidget *menuItem;
	XfceTrayIcon *icon;
	GtkWidget *menu;
	GdkPixbuf *pb;

	menu = gtk_menu_new();

	/* */
	menuItem = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES,
			NULL);
	g_signal_connect(menuItem, "activate", 
			G_CALLBACK(show_preferences_cb), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuItem);
	gtk_widget_show(menuItem);

	/* */
	menuItem = gtk_image_menu_item_new_with_mnemonic(_("Session control"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuItem),
		gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO,
			GTK_ICON_SIZE_MENU));
	g_signal_connect_swapped(menuItem, "activate",
			G_CALLBACK(gtk_widget_show), sessionControl);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuItem);
	gtk_widget_show_all(menuItem);

	/* */
	menuItem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuItem);
	gtk_widget_show(menuItem);

	/* */
	menuItem = gtk_image_menu_item_new_with_mnemonic(_("Save session"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuItem),
		gtk_image_new_from_stock(GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU));
	g_signal_connect(G_OBJECT(menuItem), "activate",
			G_CALLBACK(save_session_cb), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuItem);
	gtk_widget_show_all(menuItem);

	/* */
	menuItem = gtk_image_menu_item_new_with_mnemonic(_("Quit session"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuItem),
		gtk_image_new_from_stock(GTK_STOCK_QUIT, GTK_ICON_SIZE_MENU));
	g_signal_connect(G_OBJECT(menuItem), "activate",
			G_CALLBACK(quit_session_cb), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuItem);
	gtk_widget_show_all(menuItem);

	pb = inline_icon_at_size(session_icon_data, 16, 16);
	icon = xfce_tray_icon_new_with_menu_from_pixbuf(menu, pb);
	g_object_unref(pb);

	/* connect the double action */
	g_signal_connect_swapped(G_OBJECT(icon), "clicked",
			G_CALLBACK(toggle_visible_cb), sessionControl);

	return(icon);
}

/*
 * Check UNIX signal state
 */
static gboolean
check_signal_state(void)
{
	switch (signalState) {
	case SIGNAL_SAVE:
		save_session_cb();
		break;

	case SIGNAL_QUIT:
		quit_session_cb();
		break;
	}

	/* reset UNIX signal state */
	signalState = SIGNAL_NONE;

	/* keep checker running */
	return(TRUE);
}

/*
 * UNIX signal handler
 */
static void
signal_handler(int signalCode)
{
	switch (signalCode) {
	case SIGUSR1:
		signalState = SIGNAL_SAVE;
		break;

	default:
		signalState = SIGNAL_QUIT;
		break;
	}
}

/*
 */
int
main(int argc, char **argv)
{
	extern gchar *startupSplashTheme;
  gboolean disable_tcp = FALSE;
#ifdef HAVE_SIGACTION
	struct sigaction act;
#endif
	McsSetting *setting;
	const gchar *theme;
	gchar *message;
	
	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	gtk_init(&argc, &argv);

  for (++argv; --argc > 0; ++argv) {
    if (strcmp(*argv, "--version") == 0) {
      printf(
          "XFce %s\n"
          "\n"
          "Copyright (c) 2003,2004\n"
          "        The XFce development team. All rights reserved.\n"
          "\n"
          "Please report bugs to <%s>.\n",
          PACKAGE_STRING, PACKAGE_BUGREPORT);
      return(EXIT_SUCCESS);
    }
    else if (strcmp(*argv, "--disable-tcp") == 0) {
#ifdef HAVE__ICETRANSNOLISTEN
      disable_tcp = TRUE;
#else
      g_warning(
          "_IceTransNoListen() not available on your system, "
          "--disable-tcp has no effect!");
#endif
    }
    else {
      fprintf(stderr,
          "Usage: xfce4-session [OPTION...]\n"
          "\n"
          "GTK+\n"
          "  --display=DISPLAY        X display to use\n"
          "  --screen=SCREEN          X screen to use\n"
          "\n"
          "Application options\n"
          "  --disable-tcp            Disable binding to TCP ports\n"
          "  --help                   Print this help message\n"
          "  --version                Print version information and exit\n"
          "\n");
      return(strcmp(*argv, "--help") == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    }
  }

	/*
	 * fake a clientID for the manager, so smproxy does not recognize
	 * us to be a session client
	 */
	gdk_set_sm_client_id(manager_generate_client_id(NULL));

	/* run a sanity check before we start the actual session manager */
	if (!sanity_check(&message)) {
		xfce_err("%s", message);
		return(EXIT_FAILURE);
	}

	if (!manager_init(disable_tcp))
    return(EXIT_FAILURE);

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

	/* */
	trayIcon = create_tray_icon();
	xfce_tray_icon_connect(trayIcon);

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
		mcs_setting_free(setting);
	}
	else {
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
	 * Connect UNIX signals
	 */
#ifdef HAVE_SIGACTION
	act.sa_handler = signal_handler;
	sigemptyset(&act.sa_mask);
#ifdef SA_RESTART
	act.sa_flags = SA_RESTART;
#else
	act.sa_flags = 0;
#endif
	(void)sigaction(SIGUSR1, &act, NULL);
	(void)sigaction(SIGINT, &act, NULL);
#else
	(void)signal(SIGUSR1, signal_handler);
	(void)signal(SIGINT, signal_handler);
#endif

	/* schedule add UNIX signal state checker */
	(void)g_timeout_add(500, (GSourceFunc)check_signal_state, NULL);

	gtk_main();

	if (shutdownSave && !manager_save())
		g_warning("Unable to save session");

	return(shutdown(shutdownType));
}

