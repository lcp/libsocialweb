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
client_view_item_added_cb (MojitoClientView *view,
                           MojitoItem       *item,
                           gpointer          userdata)
{
  GList *l;

  g_debug ("New item: source = %s uuid = %s",
           item->source,
           item->uuid);

  g_debug ("Current list looks like:");

  for (l = mojito_client_view_get_sorted_items (view); l; l = l->next)
  {
    g_debug ("%s %s", 
             ((MojitoItem *)l->data)->source,
             ((MojitoItem *)l->data)->uuid);
  }
}

static void
client_open_view_cb (MojitoClient *client, 
                     MojitoClientView   *view,
                     gpointer      userdata)
{
  mojito_client_view_start (view);
  g_signal_connect (view, "item-added", G_CALLBACK (client_view_item_added_cb), NULL);
}

static void
client_get_sources_cb (MojitoClient *client,
                       GList        *sources,
                       gpointer      userdata)
{
  GList *l;

  for (l = sources; l; l = l->next)
  {
    g_debug ("Told about source: %s", l->data);
  }

  mojito_client_open_view (client, sources, 10, client_open_view_cb, NULL);
}

int
main (int    argc,
      char **argv)
{
  MojitoClient *client;
  GMainLoop *loop;

  g_type_init ();

  client = mojito_client_new ();
  mojito_client_get_sources (client, client_get_sources_cb, NULL);

  loop = g_main_loop_new (NULL, FALSE);

  g_main_loop_run (loop);
}
