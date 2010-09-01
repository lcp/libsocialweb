/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2010 Intel Corporation.
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

#include <glib-object.h>
#include <libsocialweb-client/sw-client.h>

static GMainLoop *loop;

static void
ready_cb (GObject *source_object,
	  GAsyncResult *res,
	  gpointer user_data)
{
  GError *error = NULL;

  if (sw_client_service_upload_photo_finish (SW_CLIENT_SERVICE (source_object), res, &error) == FALSE) {
    g_warning ("An error occured uploading the file: %s", error->message);
  } else {
    g_message ("File successfully uploaded");
  }

  g_main_loop_quit (loop);
  g_main_loop_unref (loop);
  loop = NULL;
}

static void
progress_cb (goffset current_num_bytes,
	     goffset total_num_bytes,
	     gpointer user_data)
{
  g_print ("upload %f %%\n", (double) (current_num_bytes / total_num_bytes));
}

int
main (int argc, char **argv)
{
  SwClient *client;
  SwClientService *service;

  if (argc != 3) {
    g_print ("$ test-upload [service name] [FILE]\n");
    return 1;
  }

  g_thread_init (NULL);
  g_type_init ();

  loop = g_main_loop_new (NULL, TRUE);

  client = sw_client_new ();
  service = sw_client_get_service (client, argv[1]);

  //FIXME test with a title as well
  if (sw_client_service_upload_photo (service,
				  argv[2],
				  NULL, /* fields */
				  NULL,
				  progress_cb,
				  NULL,
				  ready_cb,
				  NULL) == FALSE) {
    g_warning ("Service %s doesn't support photo uploads", argv[1]);
    return 1;
  }


  g_main_loop_run (loop);

  return 0;
}
