/*
 * libsocialweb - social data store
 * Copyright (C) 2009 Intel Corporation.
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
#include "sw-banned.h"
#include <gio/gio.h>

/* TODO: turn this into a proper object which manages the ban list properly */

GHashTable *
sw_ban_load (void)
{
  GHashTable *hash;
  char *path;
  GFile *file;
  GFileInputStream *stream = NULL;
  GDataInputStream *dstream = NULL;
  GError *error = NULL;
  char *line;

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  path = g_build_filename (g_get_user_cache_dir (),
                           PACKAGE,
                           "banned.txt",
                           NULL);
  file = g_file_new_for_path (path);
  g_free (path);

  stream = g_file_read (file, NULL, &error);
  if (error) goto done;
  dstream = g_data_input_stream_new ((GInputStream *)stream);

  while ((line = g_data_input_stream_read_line (dstream, NULL, NULL, &error))) {
    g_hash_table_insert (hash, line, GINT_TO_POINTER (42));
  }

 done:
  if (error) {
    if (!(error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_FOUND))
      g_message ("Cannot load ban list: %s", error->message);
    g_error_free (error);
  }
  if (dstream)
    g_object_unref (dstream);
  if (stream)
    g_object_unref (stream);
  g_object_unref (file);
  return hash;
}

void
sw_ban_save (GHashTable *hash)
{
  char *path;
  GFile *file;
  GFileOutputStream *stream = NULL;
  GDataOutputStream *dstream = NULL;
  GError *error = NULL;
  GHashTableIter iter;
  gpointer key, value;

  g_return_if_fail (hash);

  path = g_build_filename (g_get_user_cache_dir (),
                           PACKAGE,
                           "banned.txt",
                           NULL);
  file = g_file_new_for_path (path);
  g_free (path);

  stream = g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error);
  if (error) goto done;
  dstream = g_data_output_stream_new ((GOutputStream *)stream);

  g_hash_table_iter_init (&iter, hash);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    if (!g_data_output_stream_put_string (dstream, key, NULL, &error))
      goto done;
    if (!g_data_output_stream_put_string (dstream, "\n", NULL, &error))
      goto done;
  }

 done:
  if (error) {
    g_message ("Cannot save ban list: %s", error->message);
    g_error_free (error);
  }
  if (dstream)
    g_object_unref (dstream);
  if (stream)
    g_object_unref (stream);
  g_object_unref (file);
}
