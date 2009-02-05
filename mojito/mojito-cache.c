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
#include <glib/gstdio.h>

#include "mojito-cache.h"
#include "mojito-item.h"

static char *
get_cache_name (MojitoService *service)
{
  char *path, *filename;

  g_assert (service);

  /* TODO: use GIO */

  path = g_build_filename (g_get_user_cache_dir (),
                           "mojito",
                           "cache",
                           NULL);
  if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
    int err;
    err = g_mkdir_with_parents (path, 0777);
    if (err)
      g_message ("Cannot create cache directory: %s", g_strerror (err));
  }

  filename = g_build_filename (path,
                               mojito_service_get_name (service),
                               NULL);
  g_free (path);

  return filename;
}

static void
cache_set (gpointer data, gpointer user_data)
{
  MojitoItem *item = data;
  GKeyFile *keys = user_data;
  const char *group, *key, *value;
  GHashTableIter iter;

  group = mojito_item_get (item, "id");
  if (group == NULL)
    return;

  g_hash_table_iter_init (&iter, mojito_item_peek_hash (item));
  while (g_hash_table_iter_next (&iter, (gpointer)&key, (gpointer)&value)) {
    g_key_file_set_string (keys, group, key, value);
  }
}

void
mojito_cache_save (MojitoService *service, MojitoSet *set)
{
  char *filename;

  g_return_if_fail (MOJITO_IS_SERVICE (service));

  filename = get_cache_name (service);

  if (set == NULL || mojito_set_is_empty (set)) {
    g_remove (filename);
  } else {
    GKeyFile *keys;
    char *data;
    GError *error = NULL;

    keys = g_key_file_new ();
    mojito_set_foreach (set, cache_set, keys);

    data = g_key_file_to_data (keys, NULL, NULL);
    if (!g_file_set_contents (filename, data, -1, &error)) {
      g_message ("Cannot write cache: %s", error->message);
      g_error_free (error);
    }
    g_free (data);
  }

  g_free (filename);
}

static MojitoItem *
cache_load (MojitoService *service, GKeyFile *keyfile, const char *group)
{
  MojitoItem *item = NULL;
  char **keys;
  int i, count;

  keys = g_key_file_get_keys (keyfile, group, &count, NULL);
  if (keys) {
    item = mojito_item_new ();
    mojito_item_set_service (item, service);
    for (i = 0; i < count; i++) {
      mojito_item_take (item, keys[i], g_key_file_get_string (keyfile, group, keys[i], NULL));
    }
  }
  g_strfreev (keys);
  return item;
}

MojitoSet *
mojito_cache_load (MojitoService *service)
{
  char *filename;
  GKeyFile *keys;
  MojitoSet *set = NULL;

  g_return_val_if_fail (MOJITO_IS_SERVICE (service), NULL);

  keys = g_key_file_new ();

  filename = get_cache_name (service);

  if (g_key_file_load_from_file (keys, filename, G_KEY_FILE_NONE, NULL)) {
    char **groups;
    int i, count;

    groups = g_key_file_get_groups (keys, &count);
    if (count) {
      set = mojito_item_set_new ();
      for (i = 0; i < count; i++) {
        mojito_set_add (set, (GObject*)cache_load (service, keys, groups[i]));
      }
    }

    g_strfreev (groups);

  }

  g_free (filename);

  return set;
}
