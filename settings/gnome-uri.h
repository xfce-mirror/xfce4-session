/* $Id$ */

#ifndef __GNOME_URI_H__
#define __GNOME_URI_H__

#include <glib.h>

void gnome_uri_list_free_strings (GList *list);
GList *gnome_uri_list_extract_uris (const gchar *uri_list);
GList *gnome_uri_list_extract_filenames (const gchar *uri_list);

#endif /* !__GNOME_URI_H__ */
