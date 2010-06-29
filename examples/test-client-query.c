/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <libsocialweb-client/sw-client.h>
#include <libsocialweb-client/sw-client-service.h>

static SwClient *client = NULL;
static SwClientService *service = NULL;

static void
_view_items_added_cb (SwClientItemView *item_view,
                      GList                *items,
                      gpointer              userdata)
{
  GList *l;

  for (l = items; l; l = l->next)
  {
    GHashTableIter iter;
    gpointer key, value;
    SwItem *item = (SwItem *)l->data;

    g_debug (G_STRLOC ": Got item with uuid: %s", item->uuid);

    g_hash_table_iter_init (&iter, item->props);
    while (g_hash_table_iter_next (&iter, &key, &value)) 
    {
      g_debug (G_STRLOC ":     %s = %s", (gchar *)key, (gchar *)value);
    }
  }
}

static void
_query_open_view_cb (SwClientService  *service,
                     SwClientItemView *item_view,
                     gpointer              userdata)
{
  g_signal_connect (item_view,
                    "items-added",
                    (GCallback)_view_items_added_cb,
                    NULL);
  sw_client_item_view_start (item_view);
}

int
main (int    argc,
      char **argv)
{
  GMainLoop *loop;

  g_type_init ();

  client = sw_client_new ();
  service = sw_client_get_service (client, "twitter");
  sw_client_service_query_open_view (service,
                                     "feed",
                                     NULL,
                                     _query_open_view_cb,
                                     NULL);
  loop = g_main_loop_new (NULL, FALSE);

  g_main_loop_run (loop);
}

