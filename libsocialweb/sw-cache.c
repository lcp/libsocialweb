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

#include <config.h>
#include <glib/gstdio.h>

#include "sw-cache.h"
#include "sw-item.h"
#include "sw-utils.h"

/*
 * Get the file name of the cache file for this service.  As a side-effect it
 * creates the parent directory if required.
 */
static char *
get_cache_filename (SwService   *service,
                    const gchar *query,
                    GHashTable  *params)
{
  char *path, *param_hash, *filename, *full_filename;

  g_assert (service);

  /* TODO: use GIO */

  path = g_build_filename (g_get_user_cache_dir (),
                           PACKAGE,
                           "cache",
                           NULL);
  if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
    int err;
    err = g_mkdir_with_parents (path, 0777);
    if (err)
      g_message ("Cannot create cache directory: %s", g_strerror (err));
  }

  param_hash = sw_hash_string_dict (params);
  filename = g_strconcat (sw_service_get_name (service),
                          "-", 
                          query,
                          "-",
                          param_hash, NULL);
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
                             PACKAGE, "thumbnails", value,
                             NULL);

  } else {
    return NULL;
  }
}

/*
 * Create a new group in the keyfile based on the SwItem.
 */
static void
set_keyfile_from_item (gpointer data, gpointer user_data)
{
  SwItem *item = data;
  GKeyFile *keys = user_data;
  const char *group, *key, *value;
  GHashTableIter iter;

  group = sw_item_get (item, "id");
  if (group == NULL)
    return;

  /* Skip items that are not ready. Their properties will not be intact */
  if (!sw_item_get_ready (item))
    return;

  /* Set a magic field saying that this item is cached */
  g_key_file_set_string (keys, group, "cached", "1");

  g_hash_table_iter_init (&iter, sw_item_peek_hash (item));
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
 * sw_cache_save:
 *
 * @service: The service the item set is for
 * @query: The query that this cache represents
 * @params: A set of parameters (strings) that can be used by the service to
 * differentiate between different service functionality
 * @set: The set of items to cache
 *
 * Cache the items in @set to disk.
 */
void
sw_cache_save (SwService   *service,
               const gchar *query,
               GHashTable  *params,
               SwSet       *set)
{
  char *filename;

  g_return_if_fail (SW_IS_SERVICE (service));

  if (query == NULL)
    query = "feed";

  filename = get_cache_filename (service, query, params);

  if (set == NULL || sw_set_is_empty (set)) {
    g_remove (filename);
  } else {
    GKeyFile *keys;
    char *data;
    GError *error = NULL;

    keys = g_key_file_new ();
    sw_set_foreach (set, set_keyfile_from_item, keys);

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
 * From @keyfile load the items in @group and create a new #Switem for
 * @service.
 */
static SwItem *
load_item_from_keyfile (SwService  *service,
                        GKeyFile   *keyfile,
                        const char *group)
{
  SwItem *item = NULL;
  char **keys;
  gsize i, count;

  keys = g_key_file_get_keys (keyfile, group, &count, NULL);
  if (keys) {
    item = sw_item_new ();
    sw_item_set_service (item, service);
    for (i = 0; i < count; i++) {
      char *value, *new_value;

      value = g_key_file_get_string (keyfile, group, keys[i], NULL);
      /*
       * Make the cached relative paths absolute so that the client doesn't have
       * to know any internal details.
       */
      new_value = make_absolute_path (keys[i], value);

      if (new_value) {
        sw_item_take (item, keys[i], new_value);
        g_free (value);
      } else {
        sw_item_take (item, keys[i], value);
      }
    }
  }

  g_strfreev (keys);

  if (sw_service_is_uid_banned (service,
                                sw_item_get (item,
                                             "id")))
  {
    g_object_unref (item);

    return NULL;
  }

  return item;
}

/**
 * sw_cache_load:
 *
 * @service: The service to read the cache for
 * @query: The query for this cache
 * @params: A set of parameters (strings) that can be used by the service to
 * differentiate between different service functionality
 *
 * Load the cache for @service from disk, returning a #SwSet if there was a
 * cache.
 */
SwSet *
sw_cache_load (SwService   *service,
               const gchar *query,
               GHashTable  *params)
{
  char *filename;
  GKeyFile *keys;
  SwSet *set = NULL;

  g_return_val_if_fail (SW_IS_SERVICE (service), NULL);

  if (query == NULL)
    query = "feed";

  keys = g_key_file_new ();

  filename = get_cache_filename (service, query, params);

  if (g_key_file_load_from_file (keys, filename, G_KEY_FILE_NONE, NULL)) {
    char **groups;
    gsize i, count;

    groups = g_key_file_get_groups (keys, &count);

    if (count) {
      set = sw_item_set_new ();

      for (i = 0; i < count; i++) {
        SwItem *item;

        /* May be null if it's banned */
        item = load_item_from_keyfile (service, keys, groups[i]);

        if (item)
          sw_set_add (set, (GObject *)item);
      }
    }

    g_strfreev (groups);

  }

  g_free (filename);

  return set;
}

/**
 * sw_cache_drop:
 *
 * @service: The service to read the cache for
 * @query: The query for this cache
 * @params: A set of parameters (strings) that can be used by the service to
 * differentiate between different service functionality
 *
 * Free the cache for @service from disk.
 */
void
sw_cache_drop (SwService   *service,
               const gchar *query,
               GHashTable  *params)
{
  char *filename;

  g_return_if_fail (SW_IS_SERVICE (service));

  if (query == NULL)
    query = "feed";

  filename = get_cache_filename (service, query, params);

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
  s = make_relative_path ("thumbnail", "/home/ross/.cache/libsocialweb/thumbnails/1234");
  g_assert_cmpstr (s, ==, "1234");

  s = make_relative_path ("authoricon", "/home/ross/.cache/libsocialweb/thumbnails/abcd");
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
  expected = g_build_filename (g_get_user_cache_dir (), PACKAGE, "thumbnails", "1234", NULL);
  g_assert_cmpstr (s, ==, expected);

  s = make_absolute_path ("authoricon", "abcd");
  g_assert (s != NULL);
  expected = g_build_filename (g_get_user_cache_dir (), PACKAGE, "thumbnails", "abcd", NULL);
  g_assert_cmpstr (s, ==, expected);
}
#endif
