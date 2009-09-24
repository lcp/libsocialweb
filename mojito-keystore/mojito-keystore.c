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
#include "mojito-keystore.h"

typedef struct {
  const char *service;
  const char *key;
  const char *secret;
} Keys;

static const Keys keys[] = {
#ifdef FLICKR_APIKEY
  { "flickr", FLICKR_APIKEY, FLICKR_SECRET },
#endif
#ifdef LASTFM_APIKEY
  { "lastfm", LASTFM_APIKEY, NULL },
#endif
#ifdef MYSPACE_APIKEY
  { "myspace", MYSPACE_APIKEY, MYSPACE_SECRET },
#endif
#ifdef TWITTER_APIKEY
  { "twitter", TWITTER_APIKEY, TWITTER_SECRET },
#endif
#ifdef DIGG_APIKEY
  { "digg", DIGG_APIKEY, NULL },
#endif
  { NULL }
};

gboolean
mojito_keystore_get_key_secret (const char *service, const char **key, const char **secret)
{
  const Keys *k;

  g_return_val_if_fail (service, FALSE);
  g_return_val_if_fail (key, FALSE);
  /* secret can be NULL because some services don't have or need a secret */

  for (k = keys; k->service; k++) {
    if (strcmp (k->service, service) == 0) {
      *key = k->key;
      if (secret)
        *secret = k->secret;
      return TRUE;
    }
  }

#if ! BUILD_TESTS
  /* Disable this for the tests because it gets in the way */
  g_printerr ("Cannot find keys for service %s\n", service);
#endif

  *key = NULL;
  if (secret)
    *secret = NULL;

  return FALSE;
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
  g_assert (key == NULL);
  g_assert (secret == NULL);

  key = mojito_keystore_get_key ("foobar");
  g_assert (key == NULL);
}

static void
test_key_secret (void)
{
  const char *key, *secret;
  gboolean ret;

  key = secret = NULL;
  ret = mojito_keystore_get_key_secret ("flickr", &key, &secret);
  g_assert (ret == TRUE);
  g_assert_cmpstr (key, ==, FLICKR_APIKEY);
  g_assert_cmpstr (secret, ==, FLICKR_SECRET);

  key = secret = NULL;
  ret = mojito_keystore_get_key_secret ("lastfm", &key, &secret);
  g_assert (ret == TRUE);
  g_assert_cmpstr (key, ==, LASTFM_APIKEY);
  g_assert (secret == NULL);

  key = secret = NULL;
  ret = mojito_keystore_get_key_secret ("myspace", &key, &secret);
  g_assert (ret == TRUE);
  g_assert_cmpstr (key, ==, MYSPACE_APIKEY);
  g_assert_cmpstr (secret, ==, MYSPACE_SECRET);

  key = secret = NULL;
  ret = mojito_keystore_get_key_secret ("twitter", &key, &secret);
  g_assert (ret == TRUE);
  g_assert_cmpstr (key, ==, TWITTER_APIKEY);
  g_assert_cmpstr (secret, ==, TWITTER_SECRET);

  key = secret = NULL;
  ret = mojito_keystore_get_key_secret ("digg", &key, &secret);
  g_assert (ret == TRUE);
  g_assert_cmpstr (key, ==, DIGG_APIKEY);
  g_assert (secret == NULL);
}

static void
test_key (void)
{
  const char *key;

  key = mojito_keystore_get_key ("flickr");
  g_assert_cmpstr (key, ==, FLICKR_APIKEY);

  key = mojito_keystore_get_key ("lastfm");
  g_assert_cmpstr (key, ==, LASTFM_APIKEY);

  key = mojito_keystore_get_key ("myspace");
  g_assert_cmpstr (key, ==, MYSPACE_APIKEY);

  key = mojito_keystore_get_key ("twitter");
  g_assert_cmpstr (key, ==, TWITTER_APIKEY);

  key = mojito_keystore_get_key ("digg");
  g_assert_cmpstr (key, ==, DIGG_APIKEY);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/keystore/invalid", test_invalid);
  g_test_add_func ("/keystore/key_secret", test_key_secret);
  g_test_add_func ("/keystore/key", test_key);

  return g_test_run ();
}
#endif
