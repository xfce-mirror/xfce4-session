/*
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <xfconf/xfconf.h>

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "xfae-window.h"
#include "xfce4-session-settings-common.h"
#include "xfce4-session-settings_ui.h"


static GdkNativeWindow opt_socket_id = 0;
static gboolean opt_version = FALSE;
static GOptionEntry option_entries[] =
{
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};

static void xfce4_session_settings_dialog_response (GtkDialog *dialog, gint response, gpointer userdata)
{
    if (response == GTK_RESPONSE_HELP) {
       xfce_dialog_show_help (GTK_WINDOW (dialog), "xfce4-session", "preferences", NULL);
    }
    else {
      gtk_widget_destroy(GTK_WIDGET(dialog));
      gtk_main_quit ();
    }
}

int
main(int argc,
     char **argv)
{
    GtkBuilder *builder;
    GtkWidget *notebook, *xfae_page, *lbl;
    GError *error = NULL;
    XfconfChannel *channel;

    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    if(!gtk_init_with_args (&argc, &argv, "", option_entries,
                            GETTEXT_PACKAGE, &error))
    {
        if(G_LIKELY(error)) {
            g_print("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print(_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print("\n");
            g_error_free (error);
        } else
            g_error("Unable to open display.");

        return EXIT_FAILURE;
    }

    if(G_UNLIKELY(opt_version)) {
        g_print("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
        g_print("%s\n", "Copyright (c) 2004-2014");
        g_print("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print(_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print("\n");

        return EXIT_SUCCESS;
    }

    if(G_UNLIKELY(!xfconf_init(&error))) {
        xfce_dialog_show_error (NULL,
                                error,
                                _("Unable to contact settings server"));
        g_error_free(error);
        return EXIT_FAILURE;
    }

    gtk_window_set_default_icon_name("xfce4-session");

    /* hook to make sure the libxfce4ui library is linked */
    if (xfce_titled_dialog_get_type() == 0)
        return EXIT_FAILURE;

    builder = gtk_builder_new();
    gtk_builder_add_from_string(builder,
                                xfce4_session_settings_ui,
                                xfce4_session_settings_ui_length,
                                &error);

    if(!builder) {
        xfce_dialog_show_error(NULL, error,
                               _("Unable to create user interface from embedded definition data"));
        g_error_free (error);
        return EXIT_FAILURE;
    }

    splash_settings_init(builder);
    session_editor_init(builder);

    /* FIXME: someday, glade-ify this, maybe. */
    xfae_page = xfae_window_new();
    gtk_widget_show(xfae_page);
    notebook = GTK_WIDGET(gtk_builder_get_object(builder, "plug-child"));
    lbl = gtk_label_new_with_mnemonic(_("App_lication Autostart"));
    gtk_widget_show(lbl);
    gtk_notebook_insert_page(GTK_NOTEBOOK(notebook), xfae_page, lbl, 2);

    channel = xfconf_channel_get(SETTINGS_CHANNEL);

    /* bind widgets to xfconf */
    xfconf_g_property_bind(channel, "/chooser/AlwaysDisplay", G_TYPE_BOOLEAN,
                           gtk_builder_get_object(builder, "chk_display_chooser"),
                           "active");
    xfconf_g_property_bind(channel, "/general/AutoSave", G_TYPE_BOOLEAN,
                           gtk_builder_get_object(builder, "chk_session_autosave"),
                           "active");
    xfconf_g_property_bind(channel, "/general/PromptOnLogout", G_TYPE_BOOLEAN,
                           gtk_builder_get_object(builder, "chk_logout_prompt"),
                           "active");
    xfconf_g_property_bind(channel, "/compat/LaunchGNOME", G_TYPE_BOOLEAN,
                           gtk_builder_get_object(builder, "chk_compat_gnome"),
                           "active");
    xfconf_g_property_bind(channel, "/compat/LaunchKDE", G_TYPE_BOOLEAN,
                           gtk_builder_get_object(builder, "chk_compat_kde"),
                           "active");
    xfconf_g_property_bind(channel, "/security/EnableTcp", G_TYPE_BOOLEAN,
                           gtk_builder_get_object(builder, "chk_enable_tcp"),
                           "active");
    xfconf_g_property_bind(channel, "/shutdown/LockScreen", G_TYPE_BOOLEAN,
                           gtk_builder_get_object(builder, "chk_lock_screen"),
                           "active");

    if(G_UNLIKELY(opt_socket_id == 0)) {
        GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "xfce4_session_settings_dialog"));

        g_signal_connect(dialog, "response", G_CALLBACK(xfce4_session_settings_dialog_response), NULL);
        g_signal_connect(dialog, "delete-event", G_CALLBACK(gtk_main_quit), NULL);

        gtk_widget_show(dialog);

        /* To prevent the settings dialog to be saved in the session */
        gdk_set_sm_client_id ("FAKE ID");

        gtk_main();
    } else {
        GtkWidget *plug, *plug_child;

        plug = gtk_plug_new(opt_socket_id);
        gtk_widget_show(plug);
        g_signal_connect(plug, "delete-event",
                         G_CALLBACK(gtk_main_quit), NULL);

        plug_child = GTK_WIDGET(gtk_builder_get_object(builder, "plug-child"));
        gtk_widget_reparent(plug_child, plug);
        gtk_widget_show(plug_child);

        /* Stop startup notification */
        gdk_notify_startup_complete();

        gtk_main();
    }

    g_object_unref(builder);

    return EXIT_SUCCESS;
}
