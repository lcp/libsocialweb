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
#include "mojito-utils.h"

/*
 * Get the file name of the cache file for this service.  As a side-effect it
 * creates the parent directory if required.
 */
static char *
get_cache_filename (MojitoService *service, GHashTable *params)
{
  char *path, *param_hash, *filename, *full_filename;

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

  param_hash = mojito_hash_string_dict (params);
  filename = g_strconcat (mojito_service_get_name (service),
                          "-", param_hash, NULL);
  g_free (param_hash);

  full_filename = g_build_filename (path, filename, NULL);

  g_free (path);
  g_free (filename);

  return full_filename;
}

/*
 * make_relative_path and make_absolute_path are pretty damn nasty and should be
 * replaced as soon as possible.  One better solution would be to change the
 * keys to be namespaced, so that it is possible to detect keys which are paths
 * without having to hardcode the list.
 */

/*
 * Make an absolute path relative, for when saving it to the cache.
 */
static char *
make_relative_path (const char *key, const char *value)
{
  if (g_str_equal (key, "authoricon") || g_str_equal (key, "thumbnail")) {
    return g_path_get_basename (value);
  } else {
    return NULL;
  }
}

/*
 * Make a relative path absolute, for when loading from the cache.
 */
static char *
make_absolute_path (const char *key, const char *value)
{
  if (g_str_equal (key, "authoricon") || g_str_equal (key, "thumbnail")) {
    return g_build_filename (g_get_user_cache_dir (),
                             "mojito", "thumbnails", value,
                             NULL);

  } else {
    return NULL;
  }
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

  /* Skip items that are not ready. Their properties will not be intact */
  if (!mojito_item_get_ready (item))
    return;

  /* Set a magic field saying that this item is cached */
  g_key_file_set_string (keys, group, "cached", "1");

  g_hash_table_iter_init (&iter, mojito_item_peek_hash (item));
  while (g_hash_table_iter_next (&iter, (gpointer)&key, (gpointer)&value)) {
    char *new_value;
    /*
     * We make relative paths when saving so that the cache files are portable
     * between users.  This normally doesn't happen but it's useful and the
     * preloaded cache depends on this.
     */
    new_value = make_relative_path (key, value);
    if (new_value) {
      g_key_file_set_string (keys, group, key, new_value);
      g_free (new_value);
    } else {
      g_key_file_set_string (keys, group, key, value);
    }
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
  GHashTable *params = NULL;

  g_return_if_fail (MOJITO_IS_SERVICE (service));

  g_object_get (service, "params", &params, NULL);

  filename = get_cache_filename (service, params);

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
  g_hash_table_unref (params);
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
      char *value, *new_value;

      value = g_key_file_get_string (keyfile, group, keys[i], NULL);
      /*
       * Make the cached relative paths absolute so that the client doesn't have
       * to know any internal details.
       */
      new_value = make_absolute_path (keys[i], value);

      if (new_value) {
        mojito_item_take (item, keys[i], new_value);
        g_free (value);
      } else {
        mojito_item_take (item, keys[i], value);
      }
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
  GHashTable *params = NULL;
  GKeyFile *keys;
  MojitoSet *set = NULL;

  g_return_val_if_fail (MOJITO_IS_SERVICE (service), NULL);

  keys = g_key_file_new ();

  g_object_get (service, "params", &params, NULL);

  filename = get_cache_filename (service, params);

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
  g_hash_table_unref (params);

  return set;
}

void
mojito_cache_drop (MojitoService *service)
{
  char *filename;
  GHashTable *params = NULL;

  g_return_if_fail (MOJITO_IS_SERVICE (service));

  g_object_get (service, "params", &params, NULL);

  filename = get_cache_filename (service, params);

  g_hash_table_unref (params);

  g_remove (filename);

  g_free (filename);
}


#if BUILD_TESTS

#include "test-runner.h"

void
test_cache_relative (void)
{
  char *s;

  /* These keys don't get mangled */
  s = make_relative_path ("authorid", "/foo/bar");
  g_assert (s == NULL);

  s = make_relative_path ("title", "this is a test");
  g_assert (s == NULL);

  /* These keys do */
  s = make_relative_path ("thumbnail", "/home/ross/.cache/mojito/thumbnails/1234");
  g_assert_cmpstr (s, ==, "1234");

  s = make_relative_path ("authoricon", "/home/ross/.cache/mojito/thumbnails/abcd");
  g_assert_cmpstr (s, ==, "abcd");
}

void
test_cache_absolute (void)
{
  char *s, *expected;

  /* These keys don't get mangled */
  s = make_absolute_path ("authorid", "1234");
  g_assert (s == NULL);

  s = make_absolute_path ("title", "this is a test");
  g_assert (s == NULL);

  /* These keys do */
  s = make_absolute_path ("thumbnail", "1234");
  g_assert (s != NULL);
  expected = g_build_filename (g_get_user_cache_dir (), "mojito", "thumbnails", "1234", NULL);
  g_assert_cmpstr (s, ==, expected);

  s = make_absolute_path ("authoricon", "abcd");
  g_assert (s != NULL);
  expected = g_build_filename (g_get_user_cache_dir (), "mojito", "thumbnails", "abcd", NULL);
  g_assert_cmpstr (s, ==, expected);
}
#endif
