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

/*
 * Get the file name of the cache file for this service.  As a side-effect it
 * creates the parent directory if required.
 */
static char *
get_cache_filename (MojitoService *service)
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

/*
 * Create a new group in the keyfile based on the MojitoItem.
 */
static void
set_keyfile_from_item (gpointer data, gpointer user_data)
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

/**
 * mojito_cache_save:
 * @service: The service the item set is for
 * @set: The set of items to cache
 *
 * Cache the items in @set to disk.
 */
void
mojito_cache_save (MojitoService *service, MojitoSet *set)
{
  char *filename;

  g_return_if_fail (MOJITO_IS_SERVICE (service));

  filename = get_cache_filename (service);

  if (set == NULL || mojito_set_is_empty (set)) {
    g_remove (filename);
  } else {
    GKeyFile *keys;
    char *data;
    GError *error = NULL;

    keys = g_key_file_new ();
    mojito_set_foreach (set, set_keyfile_from_item, keys);

    data = g_key_file_to_data (keys, NULL, NULL);
    if (!g_file_set_contents (filename, data, -1, &error)) {
      g_message ("Cannot write cache: %s", error->message);
      g_error_free (error);
    }
    g_free (data);
  }

  g_free (filename);
}

/*
 * From @keyfile load the items in @group and create a new #Mojitoitem for
 * @service.
 */
static MojitoItem *
load_item_from_keyfile (MojitoService *service, GKeyFile *keyfile, const char *group)
{
  MojitoItem *item = NULL;
  char **keys;
  gsize i, count;

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

/**
 * mojito_cache_load:
 * @service: The service to read the cache for
 *
 * Load the cache for @service from disk, returning a #MojitoSet if there was a
 * cache, or %NULL.
 */
MojitoSet *
mojito_cache_load (MojitoService *service)
{
  char *filename;
  GKeyFile *keys;
  MojitoSet *set = NULL;

  g_return_val_if_fail (MOJITO_IS_SERVICE (service), NULL);

  keys = g_key_file_new ();

  filename = get_cache_filename (service);

  if (g_key_file_load_from_file (keys, filename, G_KEY_FILE_NONE, NULL)) {
    char **groups;
    gsize i, count;

    groups = g_key_file_get_groups (keys, &count);
    if (count) {
      set = mojito_item_set_new ();
      for (i = 0; i < count; i++) {
        mojito_set_add (set, (GObject*)load_item_from_keyfile (service, keys, groups[i]));
      }
    }

    g_strfreev (groups);

  }

  g_free (filename);

  return set;
}
