#include <string.h>
#include <mojito-keyfob/mojito-keyfob.h>
#include <rest-extras/flickr-proxy.h>
#include <gnome-keyring.h>

#define FLICKR_SERVER "http://flickr.com/"

static const GnomeKeyringPasswordSchema flickr_schema = {
  GNOME_KEYRING_ITEM_GENERIC_SECRET,
  {
    { "server", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "api-key", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { NULL, 0 }
  }
};

typedef struct {
  FlickrProxy *proxy;
  MojitoKeyfobCallback callback;
  gpointer user_data;
} CallbackData;

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
find_flickr_key_cb (GnomeKeyringResult result,
             const char *string,
             gpointer user_data)
{
  CallbackData *data = user_data;

  if (result == GNOME_KEYRING_RESULT_OK) {
    /* TODO: validate */
    flickr_proxy_set_token (data->proxy, string);
    data->callback ((RestProxy*)data->proxy, TRUE, data->user_data);
  } else {
    data->callback ((RestProxy*)data->proxy, FALSE, data->user_data);
  }

  /* Cleanup of data is done by gnome-keyring, bless it */
}

void
mojito_keyfob_flickr (FlickrProxy *proxy,
                          MojitoKeyfobCallback callback,
                          gpointer user_data)
{
  const char *key;
  CallbackData *data;

  /* TODO: hacky, make a proper singleton or proper object */
  data = g_slice_new0 (CallbackData);
  data->proxy = g_object_ref (proxy);
  data->callback = callback;
  data->user_data = user_data;

  key = flickr_proxy_get_api_key (proxy);

  gnome_keyring_find_password (&flickr_schema,
                               find_flickr_key_cb,
                               data, (GDestroyNotify)callback_data_free,
                               "server", FLICKR_SERVER,
                               "api-key", key,
                               NULL);
}

gboolean
mojito_keyfob_flickr_sync (FlickrProxy *proxy)
{
  const char *key;
  char *password = NULL;
  GnomeKeyringResult result;

  key = flickr_proxy_get_api_key (proxy);

  result = gnome_keyring_find_password_sync (&flickr_schema, &password,
                                             "server", FLICKR_SERVER,
                                             "api-key", key,
                                             NULL);

  if (result == GNOME_KEYRING_RESULT_OK) {
    /* TODO: validate so the caller doesn't need to */
    flickr_proxy_set_token (proxy, password);
    gnome_keyring_free_password (password);
    return TRUE;
  } else {
    return FALSE;
  }
}
