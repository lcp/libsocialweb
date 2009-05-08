#include <string.h>
#include <mojito-keyfob/mojito-keyfob.h>
#include <rest/oauth-proxy.h>
#include <gnome-keyring.h>

static const GnomeKeyringPasswordSchema oauth_schema = {
  GNOME_KEYRING_ITEM_GENERIC_SECRET,
  {
    { "server", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "consumer-key", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { NULL, 0 }
  }
};

typedef struct {
  OAuthProxy *proxy;
  MojitoKeyfobOAuthCallback callback;
  gpointer user_data;
} CallbackData;

/*
 * Split @string on whitespace into two strings and base-64 decode them.
 */
static gboolean
decode (const char *string, char **token, char **token_secret)
{
  char **encoded_keys;
  gboolean ret = FALSE;
  gsize len;

  g_assert (string);
  g_assert (token);
  g_assert (token_secret);

  encoded_keys = g_strsplit (string, " ", 2);

  if (encoded_keys[0] && encoded_keys[1]) {
    *token = (char*)g_base64_decode (encoded_keys[0], &len);
    *token_secret = (char*)g_base64_decode (encoded_keys[1], &len);
    ret = TRUE;
  }

  g_strfreev (encoded_keys);

  return ret;
}

static char *
encode (const char *token, const char *secret)
{
  char *encoded_token, *encoded_secret;
  char *string;

  g_assert (token);
  g_assert (secret);

  encoded_token = g_base64_encode ((guchar*)token, strlen (token));
  encoded_secret = g_base64_encode ((guchar*)secret, strlen (secret));

  string = g_strconcat (encoded_token, " ", encoded_secret, NULL);

  g_free (encoded_token);
  g_free (encoded_secret);

  return string;
}

static void
callback_data_free (CallbackData *data)
{
  g_object_unref (data->proxy);
  g_slice_free (CallbackData, data);
}

/*
 * Callback from gnome-keyring with the result of looking up the server/key
 * pair.  If this returns a secret then we can decode it and callback, otherwise
 * we have to ask the user to authenticate.
 */
static void
find_key_cb (GnomeKeyringResult result,
             const char *string,
             gpointer user_data)
{
  CallbackData *data = user_data;

  if (result == GNOME_KEYRING_RESULT_OK) {
    char *token = NULL, *token_secret = NULL;

    if (decode (string, &token, &token_secret)) {
      /*
       * TODO: is it possible to validate these tokens generically? If so then
       * it should be validated here, otherwise we need a way to clear the
       * tokens for a particular key so that re-auth works.
       */
      oauth_proxy_set_token (data->proxy, token);
      oauth_proxy_set_token_secret (data->proxy, token_secret);

      g_free (token);
      g_free (token_secret);

      data->callback (data->proxy, TRUE, data->user_data);
    } else {
      data->callback (data->proxy, FALSE, data->user_data);
    }
  } else {
    data->callback (data->proxy, FALSE, data->user_data);
  }

  /* Cleanup of data is done by gnome-keyring, bless it */
}

void
mojito_keyfob_oauth (OAuthProxy *proxy,
                          MojitoKeyfobOAuthCallback callback,
                          gpointer user_data)
{
  char *server = NULL, *key = NULL;
  CallbackData *data;

  /* TODO: hacky, make a proper singleton or proper object */
  data = g_slice_new0 (CallbackData);
  data->proxy = g_object_ref (proxy);
  data->callback = callback;
  data->user_data = user_data;

  g_object_get (proxy,
                "url-format", &server,
                "consumer-key", &key,
                NULL);

  gnome_keyring_find_password (&oauth_schema,
                               find_key_cb,
                               data, (GDestroyNotify)callback_data_free,
                               "server", server,
                               "consumer-key", key,
                               NULL);

  g_free (server);
  g_free (key);
}

gboolean
mojito_keyfob_oauth_sync (OAuthProxy *proxy)
{
  char *server = NULL, *key = NULL, *password = NULL;
  GnomeKeyringResult result;

  g_object_get (proxy,
                "url-format", &server,
                "consumer-key", &key,
                NULL);

  result = gnome_keyring_find_password_sync (&oauth_schema, &password,
                                             "server", server,
                                             "consumer-key", key,
                                             NULL);
  g_free (server);
  g_free (key);

  if (result == GNOME_KEYRING_RESULT_OK) {
    char *token = NULL, *token_secret = NULL;
    if (decode (password, &token, &token_secret)) {
      /*
       * TODO: is it possible to validate these tokens generically? If so then
       * it should be validated here, otherwise we need a way to clear the
       * tokens for a particular key so that re-auth works.
       */
      oauth_proxy_set_token (proxy, token);
      oauth_proxy_set_token_secret (proxy, token_secret);

      g_free (token);
      g_free (token_secret);
      gnome_keyring_free_password (password);

      return TRUE;
    } else {
      gnome_keyring_free_password (password);
      return FALSE;
    }
  } else {
    return FALSE;
  }
}


#if BUILD_TESTS
void
test_keyfrob_decode (void)
{
  char *token, *secret;

  token = secret = NULL;
  /* One block of invalid base64 data */
  g_assert (!decode ("1234567890", &token, &secret));
  g_assert (token == NULL);
  g_assert (secret == NULL);

  token = secret = NULL;
  /* One block of valid base64 data */
  g_assert (!decode ("MTIzNA==", &token, &secret));
  g_assert (token == NULL);
  g_assert (secret == NULL);

  token = secret = NULL;
  /* Two blocks of valid data */
  g_assert (decode ("MTIzNA== YWJjZA==", &token, &secret));
  g_assert_cmpstr (token, ==, "1234");
  g_assert_cmpstr (secret, ==, "abcd");
}

void
test_keyfrob_encode (void)
{
  g_assert_cmpstr (encode ("1234", "5678"), ==, "MTIzNA== NTY3OA==");
}

void
test_keyfrob_both (void)
{
  char *s, *token, *secret;

#define TOKEN "thisisatoken"
#define SECRET "thisisasecret"

  s = encode (TOKEN, SECRET);
  g_assert_cmpstr (s, ==, "dGhpc2lzYXRva2Vu dGhpc2lzYXNlY3JldA==");

  token = secret = NULL;
  g_assert (decode (s, &token, &secret));
  g_assert_cmpstr (token, ==, TOKEN);
  g_assert_cmpstr (secret, ==, SECRET);
}
#endif
