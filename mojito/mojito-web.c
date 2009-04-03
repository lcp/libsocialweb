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

#include <config.h>
#include <libsoup/soup.h>
#if WITH_GNOME
#include <libsoup/soup-gnome.h>
#endif

#include "mojito-web.h"

/*
 * Helper to make a known sync SoupSession with environment support.
 * TODO: should this return a ref on a singleton session?
 */
SoupSession *
mojito_web_make_sync_session (void)
{
  SoupSession *session;

  session = soup_session_sync_new ();
#if WITH_GNOME
  soup_session_add_feature_by_type (session, SOUP_TYPE_PROXY_RESOLVER_GNOME);
#endif

  return session;
}

SoupSession *
mojito_web_make_async_session (void)
{
  SoupSession *session;

  session = soup_session_async_new ();
#if WITH_GNOME
  soup_session_add_feature_by_type (session, SOUP_TYPE_PROXY_RESOLVER_GNOME);
#endif

  return session;
}

char *
mojito_web_download_image (const char *url)
{
  static SoupSession *session = NULL;
  char *md5, *path, *filename;

  g_return_val_if_fail (url, NULL);

  if (session == NULL)
    session = mojito_web_make_sync_session ();

  md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);

  path = g_build_filename (g_get_user_cache_dir (),
                           "mojito",
                           "thumbnails",
                           NULL);
  g_mkdir_with_parents (path, 0777);

  /* TODO: try and guess the extension from @url and use that */
  filename = g_build_filename (path, md5, NULL);
  g_free (md5);
  g_free (path);

  if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
    SoupMessage *msg;

    msg = soup_message_new (SOUP_METHOD_GET, url);
    soup_session_send_message (session, msg);
    if (msg->status_code == SOUP_STATUS_OK) {
      /* TODO: GError */
      g_file_set_contents (filename, msg->response_body->data, msg->response_body->length, NULL);
    } else {
      g_debug ("Cannot download %s: %s", url, msg->reason_phrase);
      g_free (filename);
      filename = NULL;
    }
    g_object_unref (msg);
  }

  return filename;
}

typedef struct {
  /* The URL we are downloading.  We own this */
  char *url;
  /* The target filename. Ownership is passed to the caller */
  char *filename;
  /* Callback */
  ImageDownloadCallback callback;
  gpointer user_data;
} AsyncData;

static void
async_download_cb (SoupSession *session, SoupMessage *msg, gpointer user_data)
{
  AsyncData *data = user_data;

  if (msg->status_code == SOUP_STATUS_OK) {
    g_file_set_contents (data->filename, msg->response_body->data, msg->response_body->length, NULL);
  } else {
    g_message ("Cannot download %s: %s", data->url, msg->reason_phrase);
    g_free (data->filename);
    data->filename = NULL;
  }

  data->callback (data->url, data->filename, data->user_data);

  /* Cleanup */
  g_free (data->url);
  g_slice_free (AsyncData, data);
}

void
mojito_web_download_image_async (const char *url, ImageDownloadCallback callback, gpointer user_data)
{
  static SoupSession *session = NULL;
  char *md5, *path, *filename;

  g_return_if_fail (url);
  g_return_if_fail (callback);

  if (session == NULL)
    session = mojito_web_make_async_session ();

  md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);

  path = g_build_filename (g_get_user_cache_dir (),
                           "mojito",
                           "thumbnails",
                           NULL);
  g_mkdir_with_parents (path, 0777);

  /* TODO: try and guess the extension from @url and use that */
  filename = g_build_filename (path, md5, NULL);
  g_free (md5);
  g_free (path);

  if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
    /* TODO: should get timestamp and make a GET call, which hopefully returns 304 */
    callback (url, filename, user_data);
  } else {
    SoupMessage *msg;
    AsyncData *data = g_slice_new0 (AsyncData);

    data->url = g_strdup (url);
    data->filename = filename;
    data->callback = callback;
    data->user_data = user_data;

    msg = soup_message_new (SOUP_METHOD_GET, url);
    soup_session_queue_message (session, msg, async_download_cb, data);
  }
}
