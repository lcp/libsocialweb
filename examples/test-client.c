/*
 * Mojito - social data store
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

#include <mojito-client/mojito-client.h>

static void
client_view_items_added_cb (MojitoClientView *view,
                            GList            *items,
                            gpointer          userdata)
{
  GList *l;
  for (l = items; l; l = l->next)
  {
    GHashTableIter iter;
    gpointer key, value;
    MojitoItem *item = (MojitoItem *)l->data;

    g_debug (G_STRLOC ": Got item with uuid: %s", item->uuid);

    g_hash_table_iter_init (&iter, item->props);
    while (g_hash_table_iter_next (&iter, &key, &value)) 
    {
      g_debug (G_STRLOC ":     %s = %s", (gchar *)key, (gchar *)value);
    }
  }
}

static void
client_open_view_cb (MojitoClient     *client,
                     MojitoClientView *view,
                     gpointer          userdata)
{
  mojito_client_view_start (view);
  g_signal_connect (view, "items-added",
                    G_CALLBACK (client_view_items_added_cb),
                    NULL);
}

static void
client_get_services_cb (MojitoClient *client,
                       const GList        *services,
                       gpointer      userdata)
{
  const GList *l;

  for (l = services; l; l = l->next)
  {
    g_print ("Told about service: %s\n", (char*)l->data);
  }

  mojito_client_open_view (client,
                           (GList*)services,
                           10,
                           client_open_view_cb,
                           NULL);
}

int
main (int    argc,
      char **argv)
{
  MojitoClient *client;
  GMainLoop *loop;

  g_type_init ();

  client = mojito_client_new ();
  mojito_client_get_services (client, client_get_services_cb, NULL);

  loop = g_main_loop_new (NULL, FALSE);

  g_main_loop_run (loop);
}
