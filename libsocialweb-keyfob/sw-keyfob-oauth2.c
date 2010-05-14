/*
 * librest - RESTful web services access
 * Copyright (c) 2008, 2009, Intel Corporation.
 * Copyright (c) 2010 Collabora.co.uk
 *
 * Authors: Rob Bradford <rob@linux.intel.com>
 *          Ross Burton <ross@linux.intel.com>
 *          Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
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
 *
 */
#include <string.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <rest/oauth2-proxy.h>
#include <gnome-keyring.h>

static const GnomeKeyringPasswordSchema oauth2_schema = {
  GNOME_KEYRING_ITEM_GENERIC_SECRET,
  {
    { "server", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { "client-id", GNOME_KEYRING_ATTRIBUTE_TYPE_STRING },
    { NULL, 0 }
  }
};

typedef struct {
  OAuth2Proxy *proxy;
  SwKeyfobCallback callback;
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
static gboolean
find_oauth2_key_cb (GnomeKeyringResult result,
                    const char *password,
                    gpointer user_data)
{
  CallbackData *data = user_data;
  gboolean retval = FALSE;

  if (result == GNOME_KEYRING_RESULT_OK) {
    gsize len = 0;
    guchar *token = g_base64_decode (password, &len);
    if (token) {
      /*
       * TODO: is it possible to validate these tokens generically? If so then
       * it should be validated here, otherwise we need a way to clear the
       * tokens for a particular key so that re-auth works.
       */
      oauth2_proxy_set_access_token (data->proxy, (const char*) token);

      g_free (token);

      retval = TRUE;
    }
  }

  if (data->callback)
    data->callback ((RestProxy*)data->proxy, retval, data->user_data);

  /* Cleanup of data is done by gnome-keyring, bless it */
  return retval;
}

void
sw_keyfob_oauth2 (OAuth2Proxy *proxy,
                  SwKeyfobCallback callback,
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
                "client-id", &key,
                NULL);

  gnome_keyring_find_password (&oauth2_schema,
                               (GnomeKeyringOperationGetStringCallback)find_oauth2_key_cb,
                               data, (GDestroyNotify)callback_data_free,
                               "server", server,
                               "client-id", key,
                               NULL);

  g_free (server);
  g_free (key);
}

gboolean
sw_keyfob_oauth2_sync (OAuth2Proxy *proxy)
{
  char *server = NULL, *key = NULL, *password = NULL;
  GnomeKeyringResult result;
  gboolean retval = FALSE;
  CallbackData *data = NULL;

  g_object_get (proxy,
                "url-format", &server,
                "client-id", &key,
                NULL);

  data = g_slice_new0 (CallbackData);
  data->proxy = g_object_ref (proxy);
  data->callback = NULL;
  data->user_data = NULL;

  result = gnome_keyring_find_password_sync (&oauth2_schema, &password,
                                             "server", server,
                                             "consumer-key", key,
                                             NULL);
  g_free (server);
  g_free (key);

  retval = find_oauth2_key_cb (result, password, data);
  callback_data_free (data);

  if (password)
    gnome_keyring_free_password (password);

  return retval;
}
