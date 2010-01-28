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

static void
is_online_cb (SwClient *client, gboolean online, gpointer user_data)
{
  g_print ("IsOnline returned %s\n", online ? "online" : "offline");
}

static void
online_cb (SwClient *client, gboolean online, gpointer user_data)
{
  g_print ("OnlineChanged notified %s\n", online ? "online" : "offline");}

int
main (int argc, char **argv)
{
  SwClient *client;
  GMainLoop *loop;

  g_type_init ();

  client = sw_client_new ();

  g_signal_connect (client, "online-changed", G_CALLBACK (online_cb), NULL);

  sw_client_is_online (client, is_online_cb, NULL);

  loop = g_main_loop_new (NULL, FALSE);

  g_main_loop_run (loop);
}
