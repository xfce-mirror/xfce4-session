/*
 * Copyright (c) 2009 Brian Tarricone <brian@tarricone.org>
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
 *
 * The session id generator was taken from the KDE session manager.
 * Copyright (c) 2000 Matthias Ettrich <ettrich@kde.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#include <dbus/dbus-glib.h>

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4panel/libxfce4panel.h>

#include "xfsm-logout-plugin-ui.h"

#define BORDER           12
#define DEFAULT_TIMEOUT  30

/* FIXME: don't copy this */
typedef enum
{
  XFSM_SHUTDOWN_ASK = 0,
  XFSM_SHUTDOWN_LOGOUT,
  XFSM_SHUTDOWN_HALT,
  XFSM_SHUTDOWN_REBOOT,
  XFSM_SHUTDOWN_SUSPEND,
  XFSM_SHUTDOWN_HIBERNATE,
} XfsmShutdownType;


typedef struct
{
    GtkActionGroup *action_group;

    GtkWidget *dialog;
    GtkWidget *dialog_label;
    gchar *dialog_text_fmt;
    guint time_left;
    guint time_left_id;
    gboolean allow_save;
} XfsmLogoutPlugin;


static void xfsm_logout_plugin_lock_screen(GtkAction *action,
                                           gpointer user_data);
static void xfsm_logout_plugin_do_something(GtkAction *action,
                                            gpointer user_data);

/* FIXME: replace with xdg spec icon names where appropriate */
static const GtkActionEntry action_entries[] =
{
    { "session-menu", NULL, N_("Session"), NULL, NULL, NULL },
    { "lock-screen", "system-lock-screen", N_("Loc_k screen"), NULL, NULL, G_CALLBACK(xfsm_logout_plugin_lock_screen) },
    { "suspend", "xfsm-suspend", N_("_Suspend"), NULL, NULL, G_CALLBACK(xfsm_logout_plugin_do_something) },
    { "hibernate", "xfsm-hibernate", N_("_Hibernate"), NULL, NULL, G_CALLBACK(xfsm_logout_plugin_do_something) },
    { "reboot", "xfsm-reboot", N_("_Reboot"), NULL, NULL, G_CALLBACK(xfsm_logout_plugin_do_something) },
    { "shutdown", "system-shutdown", N_("Shut _down"), NULL, NULL, G_CALLBACK(xfsm_logout_plugin_do_something) },
    { "logout", "system-log-out", N_("_Log out"), NULL, NULL, G_CALLBACK(xfsm_logout_plugin_do_something) },
};

static const struct
{
    const gchar *primary;
    const gchar *secondary;
    const gchar *button_text;
    const gchar *button_icon_name;
    const gchar *error_text;
} dialog_strings[] = {
    { NULL, NULL, NULL, NULL }, /* XFSM_SHUTDOWN_ASK */
    {   /* XFSM_SHUTDOWN_LOGOUT */
        N_("Are you sure you want to log out?"),
        N_("You will be logged out in %u seconds."),
        N_("_Log out"),
        "system-log-out",
        N_("Failed to log out.")
    },
    {   /* XFSM_SHUTDOWN_HALT */
        N_("Are you sure you want to shut down?"),
        N_("Your system will shut down in %u seconds."),
        N_("Shut _down"),
        "system-shutdown",
        N_("Failed to shut down.")
    },
    {   /* XFSM_SHUTDOWN_REBOOT */
        N_("Are you sure you want to reboot?"),
        N_("Your system will reboot in %u seconds."),
        N_("_Reboot"),
        "xfsm-reboot",
        N_("Failed to reboot.")
    },
    {   /* XFSM_SHUTDOWN_SUSPEND */
        NULL,
        NULL,
        N_("_Suspend"),
        "xfsm-suspend",
        N_("Failed to suspend")
    },
    {   /* XFSM_SHUTDOWN_HIBERNATE */
        NULL,
        NULL,
        N_("_Hibernate"),
        "xfsm-hibernate",
        N_("Failed to hibernate")
    }, 
};

static void
xfsm_logout_plugin_lock_screen(GtkAction *action,
                          gpointer user_data)
{
    xfce_exec("xflock4", FALSE, FALSE, NULL);
}

static gboolean
xfsm_logout_plugin_do_dbus_call(XfsmLogoutPlugin *logout_plugin,
                                XfsmShutdownType type,
                                GError **error)
{
    DBusGConnection *conn;
    DBusGProxy *proxy;
    gboolean ret;

    g_return_val_if_fail(logout_plugin != NULL, FALSE);
    g_return_val_if_fail(type >= XFSM_SHUTDOWN_LOGOUT && type <= XFSM_SHUTDOWN_HIBERNATE, FALSE);
    g_return_val_if_fail(!error || !*error, FALSE);

    conn = dbus_g_bus_get(DBUS_BUS_SESSION, error);
    if(!conn)
        return FALSE;

    proxy = dbus_g_proxy_new_for_name(conn, "org.xfce.SessionManager",
                                      "/org/xfce/SessionManager",
                                      "org.xfce.Session.Manager");
    ret = dbus_g_proxy_call(proxy, "Shutdown", error,
                            G_TYPE_UINT, type,
                            G_TYPE_BOOLEAN, logout_plugin->allow_save,
                            G_TYPE_INVALID);
    g_object_unref(G_OBJECT(proxy));

    return ret;
}

static gboolean
xfsm_logout_dialog_update_time_left(gpointer data)
{
    XfsmLogoutPlugin *logout_plugin = data;
    gchar *text;

    if(--logout_plugin->time_left == 0) {
        /* unattended shutdown, so don't allow apps to cancel shutdown */
        logout_plugin->allow_save = FALSE;
        logout_plugin->time_left_id = 0;
        gtk_dialog_response(GTK_DIALOG(logout_plugin->dialog),
                            GTK_RESPONSE_ACCEPT);
        return FALSE;
    }

    text = g_strdup_printf(logout_plugin->dialog_text_fmt,
                           logout_plugin->time_left);
    gtk_label_set_markup(GTK_LABEL(logout_plugin->dialog_label), text);
    g_free(text);

    return TRUE;
}

static void
xfsm_logout_plugin_show_confirmation_dialog(XfsmLogoutPlugin *logout_plugin,
                                            XfsmShutdownType type)
{
    gint resp = GTK_RESPONSE_ACCEPT;

    g_return_if_fail(logout_plugin != NULL);
    g_return_if_fail(type >= XFSM_SHUTDOWN_LOGOUT && type <= XFSM_SHUTDOWN_HIBERNATE);

    logout_plugin->allow_save = TRUE;

    if(type != XFSM_SHUTDOWN_SUSPEND && type != XFSM_SHUTDOWN_HIBERNATE) {
        GtkWidget *dialog, *topvbox, *hbox, *image, *label;
        GtkWidget *button, *btn_hbox, *align;
        gchar *text;

        dialog = gtk_dialog_new_with_buttons(_("Close Session"), NULL,
                                             GTK_DIALOG_NO_SEPARATOR,
                                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                             NULL);
        gtk_window_set_keep_above(GTK_WINDOW(dialog), TRUE);
        gtk_window_stick(GTK_WINDOW(dialog));
        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), TRUE);
        gtk_container_set_border_width(GTK_CONTAINER(dialog), BORDER/6);

        button = gtk_button_new();
        GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
        gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button,
                                     GTK_RESPONSE_ACCEPT);

        align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
        gtk_container_add(GTK_CONTAINER(button), align);

        btn_hbox = gtk_hbox_new(FALSE, BORDER/6);
        gtk_container_add(GTK_CONTAINER(align), btn_hbox);

        image = gtk_image_new_from_icon_name(dialog_strings[type].button_icon_name,
                                             GTK_ICON_SIZE_BUTTON);
        gtk_box_pack_start(GTK_BOX(btn_hbox), image, FALSE, FALSE, 0);

        label = gtk_label_new_with_mnemonic(_(dialog_strings[type].button_text));
        gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
        gtk_box_pack_end(GTK_BOX(btn_hbox), label, FALSE, FALSE, 0);

        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

#if GTK_CHECK_VERSION(2, 14, 0)
        topvbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
        topvbox = GTK_DIALOG(dialog)->vbox;
#endif

        hbox = gtk_hbox_new(FALSE, BORDER);
        gtk_container_set_border_width(GTK_CONTAINER(hbox), BORDER/2);
        gtk_box_pack_start(GTK_BOX(topvbox), hbox, FALSE, FALSE, 0);

        image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_QUESTION,
                                         GTK_ICON_SIZE_DIALOG);
        gtk_misc_set_alignment(GTK_MISC(image), 0.0, 0.0);
        gtk_box_pack_start(GTK_BOX(hbox), image, TRUE, FALSE, 0);

        logout_plugin->dialog_text_fmt = g_strdup_printf("<span weight='bold' size='larger'>%s</span>\n\n%s",
                                                         _(dialog_strings[type].primary),
                                                         _(dialog_strings[type].secondary));
        text = g_strdup_printf(logout_plugin->dialog_text_fmt, DEFAULT_TIMEOUT);

        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), text);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

        g_free(text);

        logout_plugin->dialog = dialog;
        logout_plugin->dialog_label = label;
        logout_plugin->time_left = DEFAULT_TIMEOUT;
        logout_plugin->time_left_id = g_timeout_add_seconds(1,
                                                            xfsm_logout_dialog_update_time_left,
                                                            logout_plugin);

        gtk_widget_show_all(dialog);
        resp = gtk_dialog_run(GTK_DIALOG(dialog));

        if(logout_plugin->time_left_id) {
            g_source_remove(logout_plugin->time_left_id);
            logout_plugin->time_left_id = 0;
        }

        g_free(logout_plugin->dialog_text_fmt);
        logout_plugin->dialog_text_fmt = NULL;

        gtk_widget_destroy(dialog);
        logout_plugin->dialog = NULL;
        logout_plugin->dialog_label = NULL;

        /* get the dialog to go away */
        while(gtk_events_pending())
            gtk_main_iteration();
    }

    if(resp == GTK_RESPONSE_ACCEPT) {
        GError *error = NULL;

        if(!xfsm_logout_plugin_do_dbus_call(logout_plugin, type, &error)) {
            xfce_message_dialog(NULL, _("Session Error"),
                                GTK_STOCK_DIALOG_ERROR,
                                _(dialog_strings[type].error_text),
                                error->message,
                                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                                NULL);
            g_error_free(error);
        }
    }
}

static void
xfsm_logout_plugin_do_something(GtkAction *action,
                                gpointer user_data)
{
    XfsmLogoutPlugin *plugin = user_data;
    const gchar *name;
    XfsmShutdownType type = XFSM_SHUTDOWN_ASK;

    name = gtk_action_get_name(action);
    if(!strcmp(name, "logout"))
        type = XFSM_SHUTDOWN_LOGOUT;
    else if(!strcmp(name, "reboot"))
        type = XFSM_SHUTDOWN_REBOOT;
    else if(!strcmp(name, "shutdown"))
        type = XFSM_SHUTDOWN_HALT;
    else if(!strcmp(name, "suspend"))
        type = XFSM_SHUTDOWN_SUSPEND;
    else if(!strcmp(name, "hibernate"))
        type = XFSM_SHUTDOWN_HIBERNATE;
    else {
        g_critical(G_STRLOC ": Invalid action \"%s\"", name);
        return;
    }

    xfsm_logout_plugin_show_confirmation_dialog(plugin, type);
}

static void
xfsm_logout_plugin_size_changed(XfcePanelPlugin *plugin,
                                gint size,
                                gpointer user_data)
{
    GtkWidget *menubar = user_data;

    if(xfce_panel_plugin_get_orientation(plugin) == GTK_ORIENTATION_HORIZONTAL)
        gtk_widget_set_size_request(menubar, -1, size);
    else
        gtk_widget_set_size_request(menubar, size, -1);
}

static void
xfsm_logout_plugin_free_data(XfcePanelPlugin *plugin,
                             gpointer user_data)
{
    XfsmLogoutPlugin *logout_plugin = user_data;

    if(logout_plugin->dialog) {
        /* this will take care of cleaning up the timeout source as
         * well as destroying the dialog */
        gtk_dialog_response(GTK_DIALOG(logout_plugin->dialog),
                            GTK_RESPONSE_CANCEL);
    }

    g_object_unref(G_OBJECT(logout_plugin->action_group));

    g_free(logout_plugin);
}

static void
xfsm_logout_plugin_construct(XfcePanelPlugin *plugin)
{
    XfsmLogoutPlugin *logout_plugin;
    GtkUIManager *uimgr;
    GtkWidget *menubar, *mi, *submenu;
#ifdef HAVE_GETPWUID
    GtkWidget *label;
    struct passwd *pwent;
#endif

    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    gdk_set_sm_client_id("fake-client-id-xfsm-logout-plugin");
    gtk_window_set_default_icon_name("xfce4-session");
    /* obey Fitt's law */
    gtk_rc_parse_string("style \"menubar-style\" {\n"
                        "    GtkMenuBar::shadow-type = GTK_SHADOW_NONE\n"
                        "}\n"
                        "class \"GtkMenuBar\" style \"menubar-style\"");

    logout_plugin = g_new0(XfsmLogoutPlugin, 1);

    logout_plugin->action_group = gtk_action_group_new("logout-plugin");
    gtk_action_group_set_translation_domain(logout_plugin->action_group,
                                            GETTEXT_PACKAGE);
    gtk_action_group_add_actions(logout_plugin->action_group, action_entries,
                                 G_N_ELEMENTS(action_entries),
                                 logout_plugin);

    uimgr = gtk_ui_manager_new();
    gtk_ui_manager_insert_action_group(uimgr, logout_plugin->action_group, 0);
    gtk_ui_manager_add_ui_from_string(uimgr, logout_plugin_ui,
                                      logout_plugin_ui_length, NULL);

    mi = gtk_ui_manager_get_widget(uimgr, "/main-menubar/session-menu");

#ifdef HAVE_GETPWUID
    pwent = getpwuid(geteuid());
    if(pwent) {
        label = gtk_bin_get_child(GTK_BIN(mi));
        gtk_label_set_text(GTK_LABEL(label), pwent->pw_name);
    }
#endif

    menubar = gtk_ui_manager_get_widget(uimgr, "/main-menubar");
    gtk_container_set_border_width(GTK_CONTAINER(menubar), 0);
    gtk_container_add(GTK_CONTAINER(plugin), menubar);
    xfce_panel_plugin_add_action_widget(plugin, menubar);

    submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(mi));
    xfce_panel_plugin_register_menu(plugin, GTK_MENU(submenu));

    /* annoyingly, handling size-changed is required even if we don't
     * do anything special.  otherwise we get truncated. */
    g_signal_connect(plugin, "size-changed",
                     G_CALLBACK(xfsm_logout_plugin_size_changed), menubar);
    g_signal_connect(plugin, "free-data",
                     G_CALLBACK(xfsm_logout_plugin_free_data), logout_plugin);

    gtk_widget_show_all(GTK_WIDGET(plugin));
    g_object_unref(G_OBJECT(uimgr));
}

#ifdef XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL  /* panel <= 4.6 */
XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL(xfsm_logout_plugin_construct)
#else  /* panel >= 4.7  */
XFCE_PANEL_PLUGIN_REGISTER(xfsm_logout_plugin_construct)
#endif
