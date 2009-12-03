/*
 * Mojito - social data store
 * Copyright (C) 2009 Intel Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include "mojito-keystore.h"

typedef struct {
  char *key;
  char *secret;
} KeyData;

static void
key_data_free (gpointer data)
{
  KeyData *keydata = data;
  g_free (keydata->key);
  g_free (keydata->secret);
  g_free (keydata);
}

static void
load_keys_from_dir (GHashTable *hash, const char *base_dir, gboolean is_base)
{
  GError *error = NULL;
  char *directory;
  GFileEnumerator *fenum;
  GFile *dir, *file;
  GFileInfo *info;

  if (is_base) {
    directory = g_build_filename (base_dir, "mojito", "keys", NULL);
    dir = g_file_new_for_path (directory);
    g_free (directory);
  } else {
    dir = g_file_new_for_path (base_dir);
  }

  fenum = g_file_enumerate_children (dir, "standard::*",
                                     G_FILE_QUERY_INFO_NONE,
                                     NULL, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
    g_error_free (error);
    goto done;
  }

  if (error) {
    g_message ("Cannot open directory: %s", error->message);
    g_error_free (error);
    goto done;
  }

  while ((info = g_file_enumerator_next_file (fenum, NULL, &error)) != NULL) {
    GFileInputStream *stream = NULL;
    GDataInputStream *dstream = NULL;
    const char *name;
    KeyData *data;

    if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR ||
        g_file_info_get_is_backup (info))
      continue;

    name = g_file_info_get_name (info);
    file = g_file_get_child (dir, name);

    stream = g_file_read (file, NULL, &error);
    if (error)
      continue;
    dstream = g_data_input_stream_new ((GInputStream *)stream);

    data = g_new0 (KeyData, 1);
    data->key = g_data_input_stream_read_line (dstream, NULL, NULL, NULL);
    if (data->key) {
      g_strstrip (data->key);
      if (data->key[0] == '\0')
        data->key = NULL;
    }

    data->secret = g_data_input_stream_read_line (dstream, NULL, NULL, NULL);
    if (data->secret) {
      g_strstrip (data->secret);
      if (data->secret[0] == '\0')
        data->secret = NULL;
    }

    g_hash_table_insert (hash, g_strdup (name), data);

    if (dstream)
      g_object_unref (dstream);
    if (stream)
      g_object_unref (stream);
    g_object_unref (file);
  }

 done:
  if (fenum)
    g_object_unref (fenum);
  g_object_unref (dir);
}

static gpointer
load_keys (gpointer data)
{
  GHashTable *hash;
  const char * const *dirs;

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, key_data_free);

#if ! BUILD_TESTS
  for (dirs = g_get_system_data_dirs (); *dirs; dirs++) {
    load_keys_from_dir (hash, *dirs, TRUE);
  }
  load_keys_from_dir (hash, g_get_user_config_dir (), TRUE);
#else
  load_keys_from_dir (hash, "test-keys", FALSE);
#endif

  return hash;
}

static GHashTable *
get_keys_hash (void)
{
  static GOnce once = G_ONCE_INIT;
  g_once (&once, load_keys, NULL);
  return once.retval;
}

gboolean
mojito_keystore_get_key_secret (const char *service, const char **key, const char **secret)
{
  KeyData *data;

  g_return_val_if_fail (service, FALSE);
  g_return_val_if_fail (key, FALSE);
  /* secret can be NULL because some services don't have or need a secret */

  data = g_hash_table_lookup (get_keys_hash (), service);
  if (data) {
    *key = data->key;
    if (secret)
      *secret = data->secret;
    return TRUE;
  } else {
#if ! BUILD_TESTS
    /* Disable this for the tests because it gets in the way */
    g_printerr ("Cannot find keys for service %s\n", service);
#endif
    *key = NULL;
    if (secret)
      *secret = NULL;
    return FALSE;
  }
}

const char *
mojito_keystore_get_key (const char *service)
{
  const char *key = NULL;

  g_return_val_if_fail (service, NULL);

  mojito_keystore_get_key_secret (service, &key, NULL);

  return key;
}

#if BUILD_TESTS
static void
test_invalid (void)
{
  const char *key = NULL, *secret = NULL;
  gboolean ret;

  ret = mojito_keystore_get_key_secret ("foobar", &key, &secret);
  g_assert (ret == FALSE);
  g_assert_cmpstr (key, ==, NULL);
  g_assert_cmpstr (secret, ==, NULL);

  key = mojito_keystore_get_key ("foobar");
  g_assert_cmpstr (key, ==, NULL);
}

static void
test_key_secret (void)
{
  const char *key, *secret;
  gboolean ret;

  key = secret = NULL;
  ret = mojito_keystore_get_key_secret ("flickr", &key, &secret);
  g_assert (ret == TRUE);
  g_assert_cmpstr (key, ==, "flickrkey");
  g_assert_cmpstr (secret, ==, "flickrsecret");

  key = secret = NULL;
  ret = mojito_keystore_get_key_secret ("lastfm", &key, &secret);
  g_assert (ret == TRUE);
  g_assert_cmpstr (key, ==, "lastfmkey");
  g_assert_cmpstr (secret, ==, NULL);

  key = secret = NULL;
  ret = mojito_keystore_get_key_secret ("myspace", &key, &secret);
  g_assert (ret == TRUE);
  g_assert_cmpstr (key, ==, "myspacekey");
  g_assert_cmpstr (secret, ==, "myspacesecret");

  key = secret = NULL;
  ret = mojito_keystore_get_key_secret ("twitter", &key, &secret);
  g_assert (ret == TRUE);
  g_assert_cmpstr (key, ==, "twitterkey");
  g_assert_cmpstr (secret, ==, "twittersecret");

  key = secret = NULL;
  ret = mojito_keystore_get_key_secret ("digg", &key, &secret);
  g_assert (ret == TRUE);
  g_assert_cmpstr (key, ==, "diggkey");
  g_assert_cmpstr (secret, ==, NULL);
}

static void
test_key (void)
{
  const char *key;

  key = mojito_keystore_get_key ("flickr");
  g_assert_cmpstr (key, ==, "flickrkey");

  key = mojito_keystore_get_key ("lastfm");
  g_assert_cmpstr (key, ==, "lastfmkey");

  key = mojito_keystore_get_key ("myspace");
  g_assert_cmpstr (key, ==, "myspacekey");

  key = mojito_keystore_get_key ("twitter");
  g_assert_cmpstr (key, ==, "twitterkey");

  key = mojito_keystore_get_key ("digg");
  g_assert_cmpstr (key, ==, "diggkey");
}

int
main (int argc, char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/keystore/invalid", test_invalid);
  g_test_add_func ("/keystore/key_secret", test_key_secret);
  g_test_add_func ("/keystore/key", test_key);

  return g_test_run ();
}
#endif
