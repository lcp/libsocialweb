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

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gconf/gconf-client.h>

#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-online.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <libsocialweb/sw-client-monitor.h>

#include <rest/oauth-proxy.h>
#include <rest/oauth-proxy-call.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>

#include <interfaces/sw-query-ginterface.h>
#include <interfaces/sw-avatar-ginterface.h>
#include <interfaces/sw-status-update-ginterface.h>
#include <interfaces/sw-photo-upload-ginterface.h>


#include "twitter.h"
#include "twitter-item-view.h"

static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void query_iface_init (gpointer g_iface, gpointer iface_data);
static void avatar_iface_init (gpointer g_iface, gpointer iface_data);
static void status_update_iface_init (gpointer g_iface, gpointer iface_data);
static void photo_upload_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceTwitter,
                         sw_service_twitter,
                         SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_QUERY_IFACE,
                                                query_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_AVATAR_IFACE,
                                                avatar_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_STATUS_UPDATE_IFACE,
                                                status_update_iface_init)
                         if (sw_keystore_get_key ("twitpic")) {
                           G_IMPLEMENT_INTERFACE (SW_TYPE_PHOTO_UPLOAD_IFACE,
                                                  photo_upload_iface_init)
                         }
);

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_TWITTER, SwServiceTwitterPrivate))

struct _SwServiceTwitterPrivate {
  gboolean inited;
  enum {
    OFFLINE,
    CREDS_INVALID,
    CREDS_VALID
  } credentials;
  RestProxy *proxy;
  RestProxy *twitpic_proxy;
  char *user_id;
  char *image_url;
  GConfClient *gconf;
  guint gconf_notify_id[2];
  char *username, *password;
  gboolean geotag_enabled;
};

#define KEY_BASE "/apps/libsocialweb/services/twitter"
#define KEY_USER KEY_BASE "/user"
#define KEY_PASS KEY_BASE "/password"

static void online_notify (gboolean online, gpointer user_data);
static void credentials_updated (SwService *service);

static void
_gconf_auth_changed_cb (GConfClient *client,
                        guint        cnxn_id,
                        GConfEntry  *entry,
                        gpointer     user_data)
{
  SwService *service = SW_SERVICE (user_data);

  SwServiceTwitter *twitter = SW_SERVICE_TWITTER (service);
  SwServiceTwitterPrivate *priv = twitter->priv;

  const char *username = NULL, *password = NULL;
  gboolean updated = FALSE;

  if (g_str_equal (entry->key, KEY_USER)) {
    if (entry->value)
      username = gconf_value_get_string (entry->value);
    if (username && username[0] == '\0')
      username = NULL;

    if (g_strcmp0 (priv->username, username) != 0) {
      priv->username = g_strdup (username);
      updated = TRUE;
    }
  } else if (g_str_equal (entry->key, KEY_PASS)) {
    if (entry->value)
      password = gconf_value_get_string (entry->value);
    if (password && password[0] == '\0')
      password = NULL;

    if (g_strcmp0 (priv->password, password) != 0) {
      priv->password = g_strdup (password);
      updated = TRUE;
    }
  }

  if (updated)
    credentials_updated (service);
}

RestXmlNode *
node_from_call (RestProxyCall *call)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *root;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_message ("Error from Twitter: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  if (root == NULL) {
    g_message ("Error from Twitter: %s",
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  return root;
}

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    CAN_VERIFY_CREDENTIALS,
    HAS_UPDATE_STATUS_IFACE,
    HAS_AVATAR_IFACE,
    HAS_PHOTO_UPLOAD_IFACE,
    HAS_BANISHABLE_IFACE,
    HAS_QUERY_IFACE,
    CAN_UPDATE_STATUS_WITH_GEOTAG,

    /* deprecated */
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    CAN_GEOTAG,

    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServiceTwitterPrivate *priv = GET_PRIVATE (service);

  static const char *no_caps[] = { NULL };
  static const char *configured_caps[] = {
    IS_CONFIGURED,
    NULL
  };
  static const char *invalid_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_INVALID,
    NULL
  };
  static const char *full_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_VALID,
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };
  static const char *full_caps_with_geotag[] = {
    IS_CONFIGURED,
    CREDENTIALS_VALID,
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    CAN_GEOTAG,
    NULL
  };

  switch (priv->credentials)
  {
    case CREDS_VALID:
      return priv->geotag_enabled ? full_caps_with_geotag : full_caps;
    case CREDS_INVALID:
      return invalid_caps;
    case OFFLINE:
      if (priv->username && priv->password)
        return configured_caps;
      else
        return no_caps;
  }

  /* Just in case we fell through that switch */
  g_warning ("Unhandled credential state %d", priv->credentials);
  return no_caps;
}

static void
sanity_check_date (RestProxyCall *call)
{
  GHashTable *headers;
  SoupDate *call_date;
  const char *s;
  time_t call_time, diff;

  headers = rest_proxy_call_get_response_headers (call);
  s = g_hash_table_lookup (headers, "Date");
  if (s) {
    call_date = soup_date_new_from_string (s);
    if (call_date) {
      call_time = soup_date_to_time_t (call_date);
      diff = labs (time (NULL) - call_time);
      /* More than five minutes difference between local time and the response
         time? */
      if (diff > (60 * 5)) {
        g_warning ("%ld seconds difference between HTTP time and system time!", diff);
      }
    }
    soup_date_free (call_date);
  }
  g_hash_table_unref (headers);
}

static void
verify_cb (RestProxyCall *call,
           const GError  *error,
           GObject       *weak_object,
           gpointer       userdata)
{
  SwService *service = SW_SERVICE (weak_object);
  SwServiceTwitter *twitter = SW_SERVICE_TWITTER (service);
  SwServiceTwitterPrivate *priv = twitter->priv;
  RestXmlNode *node;

  SW_DEBUG (TWITTER, "Verified credentials");

  node = node_from_call (call);
  if (!node)
    return;

  priv->credentials = CREDS_VALID;
  priv->user_id = g_strdup (rest_xml_node_find (node, "id")->content);
  priv->image_url = g_strdup (rest_xml_node_find (node, "profile_image_url")->content);
  priv->geotag_enabled = g_str_equal (rest_xml_node_find (node, "geo_enabled")->content,
                                      "true");

  rest_xml_node_unref (node);

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));

  g_object_unref (call);
}

static void
_oauth_access_token_cb (RestProxyCall *call,
                        const GError  *error,
                        GObject       *weak_object,
                        gpointer       userdata)
{
  SwService *service = SW_SERVICE (weak_object);
  SwServiceTwitter *twitter = SW_SERVICE_TWITTER (service);

  if (error) {
    sanity_check_date (call);
    g_message ("Error: %s", error->message);

    twitter->priv->credentials = CREDS_INVALID;
    sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));

    return;
  }

  oauth_proxy_call_parse_token_reponse (OAUTH_PROXY_CALL (call));

  SW_DEBUG (TWITTER, "Got OAuth access tokens");

  g_object_unref (call);

  /* Create a TwitPic proxy using OAuth Echo */
  twitter->priv->twitpic_proxy = oauth_proxy_new_echo_proxy
    (OAUTH_PROXY (twitter->priv->proxy),
     "https://api.twitter.com/1/account/verify_credentials.json",
     "http://api.twitpic.com/2/", FALSE);

  /*
   * Despite the fact we know the credentials are fine, we check them again to
   * get the user ID and avatar.
   *
   * http://apiwiki.twitter.com/Twitter-REST-API-Method:-account verify_credentials
   */
  call = rest_proxy_new_call (twitter->priv->proxy);
  rest_proxy_call_set_function (call, "1/account/verify_credentials.xml");
  rest_proxy_call_async (call, verify_cb, (GObject*)twitter, NULL, NULL);
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwServiceTwitter *twitter = (SwServiceTwitter *)user_data;
  SwServiceTwitterPrivate *priv = twitter->priv;

  SW_DEBUG (TWITTER, "Online: %s", online ? "yes" : "no");

  /* Clear the token and token secret stored inside the proxy */
  oauth_proxy_set_token (OAUTH_PROXY (priv->proxy), NULL);
  oauth_proxy_set_token_secret (OAUTH_PROXY (priv->proxy), NULL);

  if (online) {
    if (priv->username && priv->password) {
      RestProxyCall *call;

      SW_DEBUG (TWITTER, "Getting token");

      /*
       * Here we use xAuth to transform a username and password into a OAuth
       * access token.
       *
       * http://apiwiki.twitter.com/Twitter-REST-API-Method:-oauth-access_token-for-xAuth
       */
      call = rest_proxy_new_call (priv->proxy);
      rest_proxy_call_set_function (call, "oauth/access_token");
      rest_proxy_call_add_params (call,
                                  "x_auth_mode", "client_auth",
                                  "x_auth_username", priv->username,
                                  "x_auth_password", priv->password,
                                  NULL);
      rest_proxy_call_async (call, _oauth_access_token_cb, (GObject*)twitter, NULL, NULL);
      /* Set offline for now and wait for access_token_cb to return */
      priv->credentials = OFFLINE;
    } else {
      priv->credentials = OFFLINE;
    }
  } else {
    g_free (priv->user_id);

    if (priv->twitpic_proxy) {
      g_object_unref (priv->twitpic_proxy);
      priv->twitpic_proxy = NULL;
    }

    priv->user_id = NULL;
    priv->credentials = OFFLINE;

    sw_service_emit_capabilities_changed ((SwService *)twitter,
                                          get_dynamic_caps ((SwService *)twitter));
  }
}

static void
credentials_updated (SwService *service)
{
  SW_DEBUG (TWITTER, "Credentials updated");

  /* If we're online, force a reconnect to fetch new credentials */
  if (sw_is_online ()) {
    online_notify (FALSE, service);
    online_notify (TRUE, service);
  }

  sw_service_emit_user_changed (service);
  sw_service_emit_capabilities_changed ((SwService *)service,
                                        get_dynamic_caps (service));
}

static const char *
sw_service_twitter_get_name (SwService *service)
{
  return "twitter";
}

static void
sw_service_twitter_dispose (GObject *object)
{
  SwServiceTwitterPrivate *priv = SW_SERVICE_TWITTER (object)->priv;

  sw_online_remove_notify (online_notify, object);

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->twitpic_proxy) {
    g_object_unref (priv->twitpic_proxy);
    priv->twitpic_proxy = NULL;
  }

  if (priv->gconf) {
    gconf_client_notify_remove (priv->gconf, priv->gconf_notify_id[0]);
    gconf_client_notify_remove (priv->gconf, priv->gconf_notify_id[1]);
    g_object_unref (priv->gconf);
    priv->gconf = NULL;
  }

  G_OBJECT_CLASS (sw_service_twitter_parent_class)->dispose (object);
}

static void
sw_service_twitter_finalize (GObject *object)
{
  SwServiceTwitterPrivate *priv = SW_SERVICE_TWITTER (object)->priv;

  g_free (priv->user_id);
  g_free (priv->image_url);

  g_free (priv->username);
  g_free (priv->password);

  G_OBJECT_CLASS (sw_service_twitter_parent_class)->finalize (object);
}

static void
sw_service_twitter_class_init (SwServiceTwitterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceTwitterPrivate));

  object_class->dispose = sw_service_twitter_dispose;
  object_class->finalize = sw_service_twitter_finalize;

  service_class->get_name = sw_service_twitter_get_name;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_twitter_init (SwServiceTwitter *self)
{
  SW_DEBUG (TWITTER, "new instance");

  self->priv = GET_PRIVATE (self);
  self->priv->inited = FALSE;
}

/* Initable interface */

static gboolean
sw_service_twitter_initable (GInitable    *initable,
                             GCancellable *cancellable,
                             GError      **error)
{
  SwServiceTwitter *twitter = SW_SERVICE_TWITTER (initable);
  SwServiceTwitterPrivate *priv = twitter->priv;
  const char *key = NULL, *secret = NULL;

  if (priv->inited)
    return TRUE;

  sw_keystore_get_key_secret ("twitter", &key, &secret);

  if (key == NULL || secret == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }

  priv->credentials = OFFLINE;

  sw_keystore_get_key_secret ("twitter", &key, &secret);
  priv->proxy = oauth_proxy_new (key, secret, "https://api.twitter.com/", FALSE);

  priv->gconf = gconf_client_get_default ();

  gconf_client_add_dir (priv->gconf,
                        KEY_BASE,
                        GCONF_CLIENT_PRELOAD_ONELEVEL,
                        NULL);

  priv->gconf_notify_id[0] = gconf_client_notify_add (priv->gconf,
                                                      KEY_USER,
                                                      _gconf_auth_changed_cb,
                                                      twitter,
                                                      NULL,
                                                      NULL);

  priv->gconf_notify_id[1] = gconf_client_notify_add (priv->gconf,
                                                      KEY_PASS,
                                                      _gconf_auth_changed_cb,
                                                      twitter,
                                                      NULL,
                                                      NULL);

  gconf_client_notify (priv->gconf, KEY_USER);
  gconf_client_notify (priv->gconf, KEY_PASS);

  sw_online_add_notify (online_notify, twitter);

  priv->inited = TRUE;

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface, gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_twitter_initable;
}

/* Query interface */

static const gchar *valid_queries[] = { "feed",
                                        "own",
                                        "friends-only",
                                        "x-twitter-mentions" };

static gboolean
_check_query_validity (const gchar *query)
{
  gint i = 0;

  for (i = 0; i < G_N_ELEMENTS(valid_queries); i++)
  {
    if (g_str_equal (query, valid_queries[i]))
      return TRUE;
  }

  return FALSE;
}

static void
_twitter_query_open_view (SwQueryIface          *self,
                          const gchar           *query,
                          GHashTable            *params,
                          DBusGMethodInvocation *context)
{
  SwServiceTwitterPrivate *priv = GET_PRIVATE (self);
  SwItemView *item_view;
  const gchar *object_path;

  if (!_check_query_validity (query))
  {
    dbus_g_method_return_error (context,
                                g_error_new (SW_SERVICE_ERROR,
                                             SW_SERVICE_ERROR_INVALID_QUERY,
                                             "Query '%s' is invalid",
                                             query));
    return;
  }

  item_view = g_object_new (SW_TYPE_TWITTER_ITEM_VIEW,
                            "proxy", priv->proxy,
                            "service", self,
                            "query", query,
                            "params", params,
                            NULL);

  /* Ensure the object gets disposed when the client goes away */
  sw_client_monitor_add (dbus_g_method_get_sender (context),
                         (GObject *)item_view);

  object_path = sw_item_view_get_object_path (item_view);
  sw_query_iface_return_from_open_view (context,
                                        object_path);
}

static void
query_iface_init (gpointer g_iface,
                  gpointer iface_data)
{
  SwQueryIfaceClass *klass = (SwQueryIfaceClass*)g_iface;

  sw_query_iface_implement_open_view (klass,
                                      _twitter_query_open_view);
}

/* Avatar interface */

static void
_requested_avatar_downloaded_cb (const gchar *uri,
                                 gchar       *local_path,
                                 gpointer     userdata)
{
  SwService *service = SW_SERVICE (userdata);

  sw_avatar_iface_emit_avatar_retrieved (service, local_path);
  g_free (local_path);
}

static void
_twitter_avatar_request_avatar (SwAvatarIface     *self,
                                DBusGMethodInvocation *context)
{
  SwServiceTwitterPrivate *priv = GET_PRIVATE (self);

  if (priv->image_url) {
    sw_web_download_image_async (priv->image_url,
                                     _requested_avatar_downloaded_cb,
                                     self);
  }

  sw_avatar_iface_return_from_request_avatar (context);
}

static void
avatar_iface_init (gpointer g_iface,
                   gpointer iface_data)
{
  SwAvatarIfaceClass *klass = (SwAvatarIfaceClass*)g_iface;

  sw_avatar_iface_implement_request_avatar (klass,
                                                _twitter_avatar_request_avatar);
}

/* Status Update interface */
static void
_update_status_cb (RestProxyCall *call,
                   const GError  *error,
                   GObject       *weak_object,
                   gpointer       userdata)
{
  if (error)
  {
    g_critical (G_STRLOC ": Error updating status: %s",
                error->message);
    sw_status_update_iface_emit_status_updated (weak_object, FALSE);
  } else {
    SW_DEBUG (TWITTER, G_STRLOC ": Status updated.");
    sw_status_update_iface_emit_status_updated (weak_object, TRUE);
  }
}

static void
_twitter_status_update_update_status (SwStatusUpdateIface   *self,
                                      const gchar           *msg,
                                      GHashTable            *fields,
                                      DBusGMethodInvocation *context)
{
  SwServiceTwitter *twitter = SW_SERVICE_TWITTER (self);
  SwServiceTwitterPrivate *priv = twitter->priv;
  RestProxyCall *call;

  if (!priv->user_id)
    return;

  /*
   * http://apiwiki.twitter.com/Twitter-REST-API-Method:-statuses update
   */
  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_method (call, "POST");
  rest_proxy_call_set_function (call, "1/statuses/update.xml");

  rest_proxy_call_add_param (call, "status", msg);

  if (fields)
  {
    const gchar *latitude, *longitude, *twitter_reply_to;

    latitude = g_hash_table_lookup (fields, "latitude");
    longitude = g_hash_table_lookup (fields, "longitude");

    if (latitude && longitude)
    {
      rest_proxy_call_add_params (call,
                                  "lat", latitude,
                                  "long", longitude,
                                  NULL);
    }

    twitter_reply_to = g_hash_table_lookup (fields, "x-twitter-reply-to");

    if (twitter_reply_to)
    {
      rest_proxy_call_add_param (call, "in_reply_to_status_id", twitter_reply_to);
    }
  }

  rest_proxy_call_async (call, _update_status_cb, (GObject *)self, NULL, NULL);
  sw_status_update_iface_return_from_update_status (context);
}

static void
status_update_iface_init (gpointer g_iface,
                          gpointer iface_data)
{
  SwStatusUpdateIfaceClass *klass = (SwStatusUpdateIfaceClass*)g_iface;

  sw_status_update_iface_implement_update_status (klass,
                                                  _twitter_status_update_update_status);
}

/* Photo upload interface */

static void
on_upload_tweet_cb (RestProxyCall *call,
                    const GError *error,
                    GObject *weak_object,
                    gpointer user_data)
{
  if (error)
    g_message ("Cannot post to Twitter: %s", error->message);
}

static void
on_upload_cb (RestProxyCall *call,
              const GError *error,
              GObject *weak_object,
              gpointer user_data)
{
  SwServiceTwitter *twitter = SW_SERVICE_TWITTER (weak_object);
  RestXmlNode *root;
  char *tweet;
  int opid = GPOINTER_TO_INT (user_data);

  if (error) {
    sw_photo_upload_iface_emit_photo_upload_progress (twitter, opid, -1, error->message);
    return;
  }

  /* Now post to Twitter */

  root = node_from_call (call);
  if (root == NULL || g_strcmp0 (root->name, "image") != 0) {
    sw_photo_upload_iface_emit_photo_upload_progress (twitter, opid, -1, "Unexpected response from Twitpic");
    if (root)
      rest_xml_node_unref (root);
    return;
  }

  /* This format is for tweets announcing twitpic URLs, "[tweet] [url]". */
  tweet = g_strdup_printf (_("%s %s"),
                           rest_xml_node_find (root, "text")->content,
                           rest_xml_node_find (root, "url")->content);

  call = rest_proxy_new_call (twitter->priv->proxy);
  rest_proxy_call_set_method (call, "POST");
  rest_proxy_call_set_function (call, "1/statuses/update.xml");
  rest_proxy_call_add_param (call, "status", tweet);
  rest_proxy_call_async (call, on_upload_tweet_cb, (GObject *)twitter, NULL, NULL);

  sw_photo_upload_iface_emit_photo_upload_progress (twitter, opid, 100, "");

  rest_xml_node_unref (root);
  g_free (tweet);
}

static void
_twitpic_upload_photo (SwPhotoUploadIface    *self,
                       const gchar           *filename,
                       GHashTable            *params,
                       DBusGMethodInvocation *context)
{
  SwServiceTwitter *twitter = SW_SERVICE_TWITTER (self);
  SwServiceTwitterPrivate *priv = twitter->priv;
  GError *error = NULL;
  RestProxyCall *call;
  RestParam *param;
  GMappedFile *map;
  char *title, *content_type;
  static int opid = 0;

  map = g_mapped_file_new (filename, FALSE, &error);
  if (error) {
    /* TODO */
    g_message (error->message);
    return;
  }

  /* Use the title as the tweet, and if the title isn't specified use the
     filename */
  title = g_hash_table_lookup (params, "title");
  if (title == NULL) {
    title = g_path_get_basename (filename);
  }

  call = rest_proxy_new_call (priv->twitpic_proxy);
  rest_proxy_call_set_function (call, "upload.xml");

  rest_proxy_call_add_params (call,
                              "key", sw_keystore_get_key ("twitpic"),
                              "message", title,
                              NULL);
  g_free (title);

  content_type = g_content_type_guess (filename,
                                       (const guchar*) g_mapped_file_get_contents (map),
                                       g_mapped_file_get_length (map),
                                       NULL);

  param = rest_param_new_with_owner ("media",
                                     g_mapped_file_get_contents (map),
                                     g_mapped_file_get_length (map),
                                     content_type,
                                     filename,
                                     map,
                                     (GDestroyNotify)g_mapped_file_unref);
  rest_proxy_call_add_param_full (call, param);

  g_free (content_type);

  opid++;
  rest_proxy_call_async (call,
                         on_upload_cb,
                         (GObject *)self,
                         GINT_TO_POINTER (opid),
                         NULL);
  sw_photo_upload_iface_return_from_upload_photo (context, opid);
}

static void
photo_upload_iface_init (gpointer g_iface,
                         gpointer iface_data)
{
  SwPhotoUploadIfaceClass *klass = (SwPhotoUploadIfaceClass *)g_iface;

  sw_photo_upload_iface_implement_upload_photo (klass, _twitpic_upload_photo);
}
