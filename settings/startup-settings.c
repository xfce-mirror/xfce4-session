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

#include <xfconf/xfconf.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include "xfce4-session-settings-common.h"

#define DISP_CHOOSER_PROP   "/chooser/AlwaysDisplay"

#define AUTO_SAVE_PROP      "/general/AutoSave"
#define LOGOUT_PROMPT_PROP  "/general/PromptOnLogout"

#define GNOME_SUPPORT_PROP  "/compat/LaunchGNOME"
#define KDE_SUPPORT_PROP    "/compat/LaunchKDE"

#define ENABLE_TCP_PROP     "/security/EnableTcp"

void
startup_settings_init(GladeXML *gxml)
{
    XfconfChannel *channel = xfconf_channel_get(SETTINGS_CHANNEL);

    xfconf_g_property_bind(channel, DISP_CHOOSER_PROP, G_TYPE_BOOLEAN,
                           glade_xml_get_widget(gxml, "chk_display_chooser"),
                           "active");

    xfconf_g_property_bind(channel, AUTO_SAVE_PROP, G_TYPE_BOOLEAN,
                           glade_xml_get_widget(gxml, "chk_session_autosave"),
                           "active");
    xfconf_g_property_bind(channel, LOGOUT_PROMPT_PROP, G_TYPE_BOOLEAN,
                           glade_xml_get_widget(gxml, "chk_logout_prompt"),
                           "active");

    xfconf_g_property_bind(channel, GNOME_SUPPORT_PROP, G_TYPE_BOOLEAN,
                           glade_xml_get_widget(gxml, "chk_compat_gnome"),
                           "active");
    xfconf_g_property_bind(channel, KDE_SUPPORT_PROP, G_TYPE_BOOLEAN,
                           glade_xml_get_widget(gxml, "chk_compat_kde"),
                           "active");

    xfconf_g_property_bind(channel, ENABLE_TCP_PROP, G_TYPE_BOOLEAN,
                           glade_xml_get_widget(gxml, "chk_enable_tcp"),
                           "active");
}
