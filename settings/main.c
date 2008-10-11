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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <xfconf/xfconf.h>

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "xfae-window.h"
#include "xfce4-session-settings-common.h"
#include "xfce4-session-settings_glade.h"


static GdkNativeWindow opt_socket_id = 0;
static gboolean opt_version = FALSE;
static GOptionEntry option_entries[] =
{
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};


int
main(int argc,
     char **argv)
{
    GladeXML *gxml;
    GtkWidget *notebook, *xfae_page, *lbl;
    GError *error = NULL;

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
        g_print("%s\n", "Copyright (c) 2004-2008");
        g_print("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print(_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print("\n");

        return EXIT_SUCCESS;
    }

    if(G_UNLIKELY(!xfconf_init(&error))) {
        xfce_message_dialog(NULL, _("Session Settings"),
                            GTK_STOCK_DIALOG_ERROR,
                            _("Unable to contact settings server"),
                            error->message,
                            GTK_STOCK_QUIT, GTK_RESPONSE_ACCEPT,
                            NULL);
        g_error_free(error);
        return EXIT_FAILURE;
    }

    gtk_window_set_default_icon_name("xfce4-session");

    gxml = glade_xml_new_from_buffer(xfce4_session_settings_glade,
                                     xfce4_session_settings_glade_length,
                                     NULL, NULL);
    if(!gxml) {
        xfce_message_dialog(NULL, _("Internal Error"), GTK_STOCK_DIALOG_ERROR,
                            _("Unable to create user interface from embedded definition data"),
                            _("This is likely a problem with your Xfce installation"),
                            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, NULL);
        return EXIT_FAILURE;
    }

    startup_settings_init(gxml);
    splash_settings_init(gxml);
    session_editor_init(gxml);

    /* FIXME: someday, glade-ify this, maybe. */
    xfae_page = xfae_window_new();
    gtk_widget_show(xfae_page);
    notebook = glade_xml_get_widget(gxml, "plug-child");
    lbl = gtk_label_new_with_mnemonic(_("_Application Autostart"));
    gtk_widget_show(lbl);
    gtk_notebook_insert_page(GTK_NOTEBOOK(notebook), xfae_page, lbl, 2);

    if(G_UNLIKELY(opt_socket_id == 0)) {
        GtkWidget *dialog = glade_xml_get_widget(gxml, "xfce4_session_settings_dialog");
        g_object_unref(gxml);

        while(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_HELP) {
            /* FIXME: load help */
        }

        gtk_widget_destroy (dialog);
    } else {
        GtkWidget *plug, *plug_child;

        plug = gtk_plug_new(opt_socket_id);
        gtk_widget_show(plug);
        g_signal_connect(plug, "delete-event",
                         G_CALLBACK(gtk_main_quit), NULL);

        plug_child = glade_xml_get_widget(gxml, "plug-child");
        gtk_widget_reparent(plug_child, plug);
        gtk_widget_show(plug_child);
        
        g_object_unref(gxml);

        gtk_main();
    }

    return EXIT_SUCCESS;
}
