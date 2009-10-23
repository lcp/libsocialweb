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
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "twitter.h"
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>
#include <mojito/mojito-online.h>
#include <mojito/mojito-utils.h>
#include <mojito/mojito-web.h>
#include <mojito/mojito-debug.h>
#include <mojito-keyfob/mojito-keyfob.h>
#include <mojito-keystore/mojito-keystore.h>
#include <gconf/gconf-client.h>
#include <rest/oauth-proxy.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>

G_DEFINE_TYPE (MojitoServiceTwitter, mojito_service_twitter, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_TWITTER, MojitoServiceTwitterPrivate))

#define TWITTER_USE_OAUTH 0

struct _MojitoServiceTwitterPrivate {
  enum {
    OWN,
    FRIENDS
  } type;
  gboolean running;
  RestProxy *proxy;
  char *user_id;
  char *image_url;
  GRegex *twitpic_re;
#if ! TWITTER_USE_OAUTH
  GConfClient *gconf;
  guint gconf_notify_id[2];
  char *username, *password;
#endif
};

#define KEY_BASE "/apps/mojito/services/twitter"
#define KEY_USER KEY_BASE "/user"
#define KEY_PASS KEY_BASE "/password"

#if ! TWITTER_USE_OAUTH
static void online_notify (gboolean online, gpointer user_data);
static void credentials_updated (MojitoService *service);

static void
auth_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  MojitoService *service = MOJITO_SERVICE (user_data);
  MojitoServiceTwitter *twitter = MOJITO_SERVICE_TWITTER (service);
  MojitoServiceTwitterPrivate *priv = twitter->priv;
  const char *username = NULL, *password = NULL;

  if (g_str_equal (entry->key, KEY_USER)) {
    if (entry->value)
      username = gconf_value_get_string (entry->value);
    if (username && username[0] == '\0')
      username = NULL;
    priv->username = g_strdup (username);
  } else if (g_str_equal (entry->key, KEY_PASS)) {
    if (entry->value)
      password = gconf_value_get_string (entry->value);
    if (password && password[0] == '\0')
      password = NULL;
    priv->password = g_strdup (password);
  }

  credentials_updated (service);
}
#endif

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

static char *
make_date (const char *s)
{
  struct tm tm;
  strptime (s, "%a %b %d %T %z %Y", &tm);
  return mojito_time_t_to_string (timegm (&tm));
}

/*
 * Remove trailing and leading whitespace and hyphens in an attempt to clean up
 * twitpic tweets.
 */
static void
cleanup_twitpic (char *string)
{
  guchar *start;
  size_t len;

  g_return_if_fail (string != NULL);

  for (start = (guchar*) string; *start && (g_ascii_isspace (*start) || *start == '-'); start++)
    ;

  len = strlen ((char*)start);

  g_memmove (string, start, len + 1);

  while (len--) {
    if (g_ascii_isspace ((guchar) string[len]) || string[len] == '-')
      string[len] = '\0';
    else
      break;
  }
}

static MojitoItem *
make_item (MojitoServiceTwitter *twitter, RestXmlNode *node)
{
  MojitoServiceTwitterPrivate *priv = twitter->priv;
  MojitoItem *item;
  RestXmlNode *u_node, *n;
  const char *post_id, *user_id, *user_name, *date, *content;
  char *url;
  GMatchInfo *match_info;

  u_node = rest_xml_node_find (node, "user");

  user_id = rest_xml_node_find (u_node, "screen_name")->content;

  /* For friend feeds, ignore our own tweets */
  if (priv->type == FRIENDS && g_str_equal (user_id, priv->user_id))
    return NULL;

  item = mojito_item_new ();
  mojito_item_set_service (item, (MojitoService *)twitter);

  post_id = rest_xml_node_find (node, "id")->content;
  mojito_item_put (item, "authorid", user_id);

  url = g_strdup_printf ("http://twitter.com/%s/statuses/%s", user_id, post_id);
  mojito_item_put (item, "id", url);
  mojito_item_take (item, "url", url);

  user_name = rest_xml_node_find (node, "name")->content;
  mojito_item_put (item, "author", user_name);

  content = rest_xml_node_find (node, "text")->content;
  if (g_regex_match (priv->twitpic_re, content, 0, &match_info)) {
    char *twitpic_id, *new_content;

    /* Construct the thumbnail URL and download the image */
    twitpic_id = g_match_info_fetch (match_info, 1);
    url = g_strconcat ("http://twitpic.com/show/thumb/", twitpic_id, NULL);
    mojito_item_request_image_fetch (item, "thumbnail", url);
    g_free (url);

    /* Remove the URL from the tweet and use that as the title */
    new_content = g_regex_replace (priv->twitpic_re,
                                   content, -1,
                                   0, "", 0, NULL);

    cleanup_twitpic (new_content);

    mojito_item_take (item, "title", new_content);

    /* Update the URL to point at twitpic */
    url = g_strconcat ("http://twitpic.com/", twitpic_id, NULL);
    mojito_item_take (item, "url", url);

    g_free (twitpic_id);
  } else {
    mojito_item_put (item, "content", content);
  }
  g_match_info_free (match_info);

  date = rest_xml_node_find (node, "created_at")->content;
  mojito_item_take (item, "date", make_date (date));

  n = rest_xml_node_find (u_node, "location");
  if (n && n->content)
    mojito_item_put (item, "location", n->content);

  n = rest_xml_node_find (u_node, "profile_image_url");
  if (n && n->content)
    mojito_item_request_image_fetch (item, "authoricon", n->content);


  return item;
}

static void
tweets_cb (RestProxyCall *call,
           const GError  *error,
           GObject       *weak_object,
           gpointer       userdata)
{
  MojitoServiceTwitter *service = MOJITO_SERVICE_TWITTER (weak_object);
  RestXmlNode *root, *node;
  MojitoSet *set;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  root = node_from_call (call);
  if (!root)
    return;

  set = mojito_item_set_new ();

  MOJITO_DEBUG (TWITTER, "Got tweets!");

  for (node = rest_xml_node_find (root, "status"); node; node = node->next) {
    MojitoItem *item;
    /* TODO: skip the user's own tweets */

    item = make_item (service, node);
    if (item)
      mojito_set_add (set, (GObject *)item);
  }

  mojito_service_emit_refreshed ((MojitoService *)service, set);

  /* TODO cleanup */

  rest_xml_node_unref (root);
}

static void
get_status_updates (MojitoServiceTwitter *twitter)
{
  MojitoServiceTwitterPrivate *priv = twitter->priv;
  RestProxyCall *call;

  if (!priv->user_id || !priv->running)
    return;

  MOJITO_DEBUG (TWITTER, "Got status updates");

  call = rest_proxy_new_call (priv->proxy);
  switch (priv->type) {
  case OWN:
    rest_proxy_call_set_function (call, "statuses/user_timeline.xml");
    break;
  case FRIENDS:
    rest_proxy_call_set_function (call, "statuses/friends_timeline.xml");
    break;
  }
  rest_proxy_call_async (call, tweets_cb, (GObject*)twitter, NULL, NULL);
}

static const char **
get_static_caps (MojitoService *service)
{
  static const char * caps[] = {
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (MojitoService *service)
{
  MojitoServiceTwitterPrivate *priv = GET_PRIVATE (service);
  static const char * caps[] = {
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };
  static const char * no_caps[] = { NULL };

  if (priv->user_id)
    return caps;
  else
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
  MojitoService *service = MOJITO_SERVICE (weak_object);
  MojitoServiceTwitter *twitter = MOJITO_SERVICE_TWITTER (service);
  RestXmlNode *node;

  if (error) {
    sanity_check_date (call);
    g_message ("Error: %s", error->message);
    return;
  }

  MOJITO_DEBUG (TWITTER, "Authentication verified");

  node = node_from_call (call);
  if (!node)
    return;

  twitter->priv->user_id = g_strdup (rest_xml_node_find (node, "id")->content);
  twitter->priv->image_url = g_strdup (rest_xml_node_find (node, "profile_image_url")->content);

  rest_xml_node_unref (node);

  mojito_service_emit_capabilities_changed (service, get_dynamic_caps (service));

  if (twitter->priv->running)
    get_status_updates (twitter);
}

static void
got_tokens_cb (RestProxy *proxy, gboolean authorised, gpointer user_data)
{
  MojitoServiceTwitter *twitter = MOJITO_SERVICE_TWITTER (user_data);
  MojitoServiceTwitterPrivate *priv = twitter->priv;
  RestProxyCall *call;

  if (authorised) {
    MOJITO_DEBUG (TWITTER, "Authorised");
    call = rest_proxy_new_call (priv->proxy);
    rest_proxy_call_set_function (call, "account/verify_credentials.xml");
    rest_proxy_call_async (call, verify_cb, (GObject*)twitter, NULL, NULL);
  } else {
    mojito_service_emit_refreshed ((MojitoService *)twitter, NULL);
  }
}

static void
start (MojitoService *service)
{
  MojitoServiceTwitter *twitter = (MojitoServiceTwitter*)service;

  twitter->priv->running = TRUE;
}

static void
refresh (MojitoService *service)
{
  MojitoServiceTwitter *twitter = (MojitoServiceTwitter*)service;
  MojitoServiceTwitterPrivate *priv = twitter->priv;

  if (!priv->running)
    return;

#if TWITTER_USE_OAUTH
  if (priv->user_id) {
    get_status_updates (twitter);
  } else {
    mojito_keyfob_oauth ((OAuthProxy*)priv->proxy, got_tokens_cb, service);
  }
#else
  if (priv->username && priv->password && priv->proxy)
  {
    got_tokens_cb (priv->proxy, TRUE, twitter);
  }
#endif
}

static void
_status_updated_cb (RestProxyCall *call,
                    const GError  *error,
                    GObject       *weak_object,
                    gpointer       userdata)
{
  MojitoService *service = MOJITO_SERVICE (weak_object);

  mojito_service_emit_status_updated (service, error == NULL);
}

static void
update_status (MojitoService *service, const char *msg)
{
  MojitoServiceTwitter *twitter = MOJITO_SERVICE_TWITTER (service);
  MojitoServiceTwitterPrivate *priv = twitter->priv;
  RestProxyCall *call;

  if (!priv->user_id)
    return;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_method (call, "POST");
  rest_proxy_call_set_function (call, "statuses/update.xml");

  rest_proxy_call_add_params (call,
                              "status", msg,
                              NULL);

  rest_proxy_call_async (call, _status_updated_cb, (GObject *)service, NULL, NULL);
}

static void
avatar_downloaded_cb (const gchar *uri,
                       gchar       *local_path,
                       gpointer     userdata)
{
  MojitoService *service = MOJITO_SERVICE (userdata);

  mojito_service_emit_avatar_retrieved (service, local_path);
  g_free (local_path);
}

static void
request_avatar (MojitoService *service)
{
  MojitoServiceTwitterPrivate *priv = GET_PRIVATE (service);

  if (priv->image_url) {
    mojito_web_download_image_async (priv->image_url,
                                     avatar_downloaded_cb,
                                     service);
  }
}

static void
online_notify (gboolean online, gpointer user_data)
{
  MojitoServiceTwitter *twitter = (MojitoServiceTwitter *)user_data;
  MojitoServiceTwitterPrivate *priv = twitter->priv;

  MOJITO_DEBUG (TWITTER, "Online: %s", online ? "yes" : "no");

  if (online) {
#if TWITTER_USE_OAUTH
    const char *key = NULL, *secret = NULL;

    mojito_keystore_get_key_secret ("twitter", &key, &secret);
    priv->proxy = oauth_proxy_new (key, secret, "http://twitter.com/", FALSE);
    mojito_keyfob_oauth ((OAuthProxy *)priv->proxy, got_tokens_cb, twitter);
#else
    if (priv->username && priv->password) {
      char *url;
      char *escaped_user;
      char *escaped_password;

      escaped_user = g_uri_escape_string (priv->username,
                                          NULL,
                                          FALSE);
      escaped_password = g_uri_escape_string (priv->password,
                                          NULL,
                                          FALSE);

      url = g_strdup_printf ("https://%s:%s@twitter.com/",
                             escaped_user, escaped_password);

      g_free (escaped_user);
      g_free (escaped_password);

      priv->proxy = rest_proxy_new (url, FALSE);
      g_free (url);

      got_tokens_cb (priv->proxy, TRUE, twitter);
    } else {
      mojito_service_emit_refreshed ((MojitoService *)twitter, NULL);
    }
#endif
  } else {
    if (priv->proxy) {
      g_object_unref (priv->proxy);
      priv->proxy = NULL;
    }
    g_free (priv->user_id);
    priv->user_id = NULL;

    mojito_service_emit_capabilities_changed ((MojitoService *)twitter, NULL);
  }
}

static void
credentials_updated (MojitoService *service)
{
  MOJITO_DEBUG (TWITTER, "Credentials updated");

  /* If we're online, force a reconnect to fetch new credentials */
  if (mojito_is_online ()) {
    online_notify (FALSE, service);
    online_notify (TRUE, service);
  }

  mojito_service_emit_user_changed (service);
}

static const char *
mojito_service_twitter_get_name (MojitoService *service)
{
  return "twitter";
}

static void
mojito_service_twitter_constructed (GObject *object)
{
  MojitoServiceTwitter *twitter = MOJITO_SERVICE_TWITTER (object);
  MojitoServiceTwitterPrivate *priv;

  priv = twitter->priv = GET_PRIVATE (twitter);

  if (mojito_service_get_param ((MojitoService*)twitter, "own")) {
    priv->type = OWN;
  } else {
    priv->type = FRIENDS;
  }

  priv->twitpic_re = g_regex_new ("http://twitpic.com/([A-Za-z0-9]+)", 0, 0, NULL);
  g_assert (priv->twitpic_re);

#if ! TWITTER_USE_OAUTH
  priv->gconf = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf, KEY_BASE,
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  priv->gconf_notify_id[0] = gconf_client_notify_add (priv->gconf, KEY_USER,
                                                      auth_changed_cb, twitter,
                                                      NULL, NULL);
  priv->gconf_notify_id[1] = gconf_client_notify_add (priv->gconf, KEY_PASS,
                                                      auth_changed_cb, twitter,
                                                      NULL, NULL);
  gconf_client_notify (priv->gconf, KEY_USER);
  gconf_client_notify (priv->gconf, KEY_PASS);
#endif

  mojito_online_add_notify (online_notify, twitter);
  if (mojito_is_online ()) {
    online_notify (TRUE, twitter);
  }
}

static void
mojito_service_twitter_dispose (GObject *object)
{
  MojitoServiceTwitterPrivate *priv = MOJITO_SERVICE_TWITTER (object)->priv;

  mojito_online_remove_notify (online_notify, object);

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->twitpic_re) {
    g_regex_unref (priv->twitpic_re);
    priv->twitpic_re = NULL;
  }

#if ! TWITTER_USE_OAUTH
  if (priv->gconf) {
    gconf_client_notify_remove (priv->gconf, priv->gconf_notify_id[0]);
    gconf_client_notify_remove (priv->gconf, priv->gconf_notify_id[1]);
    g_object_unref (priv->gconf);
    priv->gconf = NULL;
  }
#endif

  G_OBJECT_CLASS (mojito_service_twitter_parent_class)->dispose (object);
}

static void
mojito_service_twitter_finalize (GObject *object)
{
  MojitoServiceTwitterPrivate *priv = MOJITO_SERVICE_TWITTER (object)->priv;

  g_free (priv->user_id);
  g_free (priv->image_url);

#if ! TWITTER_USE_OAUTH
  g_free (priv->username);
  g_free (priv->password);
#endif

  G_OBJECT_CLASS (mojito_service_twitter_parent_class)->finalize (object);
}

static void
mojito_service_twitter_class_init (MojitoServiceTwitterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoServiceClass *service_class = MOJITO_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoServiceTwitterPrivate));

  object_class->constructed = mojito_service_twitter_constructed;
  object_class->dispose = mojito_service_twitter_dispose;
  object_class->finalize = mojito_service_twitter_finalize;

  service_class->get_name = mojito_service_twitter_get_name;
  service_class->start = start;
  service_class->refresh = refresh;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->update_status = update_status;
  service_class->request_avatar = request_avatar;
  service_class->credentials_updated = credentials_updated;
}

static void
mojito_service_twitter_init (MojitoServiceTwitter *self)
{
  self->priv = GET_PRIVATE (self);
}
