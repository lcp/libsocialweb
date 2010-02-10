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
#include "twitter.h"
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-online.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <gconf/gconf-client.h>
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>

#include <interfaces/sw-query-ginterface.h>
#include <interfaces/sw-avatar-ginterface.h>
#include <interfaces/sw-status-update-ginterface.h>

#include "twitter-item-view.h"

static void query_iface_init (gpointer g_iface, gpointer iface_data);
static void avatar_iface_init (gpointer g_iface, gpointer iface_data);
static void status_update_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceTwitter,
                         sw_service_twitter,
                         SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (SW_TYPE_QUERY_IFACE,
                                                query_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_AVATAR_IFACE,
                                                avatar_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_STATUS_UPDATE_IFACE,
                                                status_update_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_TWITTER, SwServiceTwitterPrivate))

struct _SwServiceTwitterPrivate {
  enum {
    OWN,
    FRIENDS,
    BOTH
  } type;
  gboolean running;
  RestProxy *proxy;
  char *user_id;
  char *image_url;
  GRegex *twitpic_re;
  GConfClient *gconf;
  guint gconf_notify_id[2];
  char *username, *password;
};

#define KEY_BASE "/apps/libsocialweb/services/twitter"
#define KEY_USER KEY_BASE "/user"
#define KEY_PASS KEY_BASE "/password"

static void online_notify (gboolean online, gpointer user_data);
static void credentials_updated (SwService *service);

static void
auth_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
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

static char *
make_date (const char *s)
{
  struct tm tm;
  strptime (s, "%a %b %d %T %z %Y", &tm);
  return sw_time_t_to_string (timegm (&tm));
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

static SwItem *
make_item (SwServiceTwitter *twitter, RestXmlNode *node)
{
  SwServiceTwitterPrivate *priv = twitter->priv;
  SwItem *item;
  RestXmlNode *u_node, *n;
  const char *post_id, *user_id, *user_name, *date, *content, *screen_name;
  char *url;
  GMatchInfo *match_info;

  u_node = rest_xml_node_find (node, "user");

  user_id = rest_xml_node_find (u_node, "id")->content;

  /* For friend only feeds, ignore our own tweets */
  if (priv->type == FRIENDS &&
      user_id && g_str_equal (user_id, priv->user_id))
  {
    return NULL;
  }

  item = sw_item_new ();
  sw_item_set_service (item, (SwService *)twitter);

  post_id = rest_xml_node_find (node, "id")->content;
  sw_item_put (item, "authorid", user_id);

  if (rest_xml_node_find (u_node, "screen_name"))
  {
    screen_name = rest_xml_node_find (u_node, "screen_name")->content;
    sw_item_put (item, "screen_name", screen_name);

    url = g_strdup_printf ("http://twitter.com/%s/statuses/%s",
                           screen_name,
                           post_id);

  } else {
    url = g_strdup_printf ("http://twitter.com/%s/statuses/%s",
                           user_id,
                           post_id);
  }

  sw_item_put (item, "id", url);
  sw_item_take (item, "url", url);

  user_name = rest_xml_node_find (node, "name")->content;
  sw_item_put (item, "author", user_name);

  content = rest_xml_node_find (node, "text")->content;
  if (g_regex_match (priv->twitpic_re, content, 0, &match_info)) {
    char *twitpic_id, *new_content;

    /* Construct the thumbnail URL and download the image */
    twitpic_id = g_match_info_fetch (match_info, 1);
    url = g_strconcat ("http://twitpic.com/show/thumb/", twitpic_id, NULL);
    sw_item_request_image_fetch (item, FALSE, "thumbnail", url);
    g_free (url);

    /* Remove the URL from the tweet and use that as the title */
    new_content = g_regex_replace (priv->twitpic_re,
                                   content, -1,
                                   0, "", 0, NULL);

    cleanup_twitpic (new_content);

    sw_item_take (item, "title", new_content);

    /* Update the URL to point at twitpic */
    url = g_strconcat ("http://twitpic.com/", twitpic_id, NULL);
    sw_item_take (item, "url", url);

    g_free (twitpic_id);
  }

  sw_item_put (item, "content", content);

  g_match_info_free (match_info);

  date = rest_xml_node_find (node, "created_at")->content;
  sw_item_take (item, "date", make_date (date));

  n = rest_xml_node_find (u_node, "location");
  if (n && n->content)
    sw_item_put (item, "location", n->content);

  n = rest_xml_node_find (u_node, "profile_image_url");
  if (n && n->content)
    sw_item_request_image_fetch (item, FALSE, "authoricon", n->content);


  return item;
}

static void
tweets_cb (RestProxyCall *call,
           const GError  *error,
           GObject       *weak_object,
           gpointer       userdata)
{
  SwServiceTwitter *service = SW_SERVICE_TWITTER (weak_object);
  RestXmlNode *root, *node;
  SwSet *set;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  root = node_from_call (call);
  if (!root)
    return;

  set = sw_item_set_new ();

  SW_DEBUG (TWITTER, "Got tweets!");

  for (node = rest_xml_node_find (root, "status"); node; node = node->next) {
    SwItem *item;
    /* TODO: skip the user's own tweets */

    item = make_item (service, node);
    if (item)
      sw_set_add (set, (GObject *)item);
  }

  sw_service_emit_refreshed ((SwService *)service, set);

  /* TODO cleanup */

  rest_xml_node_unref (root);
}

static void
get_status_updates (SwServiceTwitter *twitter)
{
  SwServiceTwitterPrivate *priv = twitter->priv;
  RestProxyCall *call;

  if (!priv->user_id || !priv->running)
    return;

  SW_DEBUG (TWITTER, "Got status updates");

  call = rest_proxy_new_call (priv->proxy);
  switch (priv->type) {
  case OWN:
    rest_proxy_call_set_function (call, "statuses/user_timeline.xml");
    break;
  case FRIENDS:
  case BOTH:
    rest_proxy_call_set_function (call, "statuses/friends_timeline.xml");
    break;
  }

  rest_proxy_call_async (call, tweets_cb, (GObject*)twitter, NULL, NULL);
}

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServiceTwitterPrivate *priv = GET_PRIVATE (service);
  static const char *full_caps[] = {
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    IS_CONFIGURED,
    NULL
  };
  static const char *configured_caps[] = {
    IS_CONFIGURED,
    NULL
  };
  static const char * no_caps[] = { NULL };

  if (priv->user_id)
    return full_caps;
  else if (priv->username && priv->password)
    return configured_caps;
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
  SwService *service = SW_SERVICE (weak_object);
  SwServiceTwitter *twitter = SW_SERVICE_TWITTER (service);
  RestXmlNode *node;

  if (error) {
    sanity_check_date (call);
    g_message ("Error: %s", error->message);
    return;
  }

  SW_DEBUG (TWITTER, "Authentication verified");

  node = node_from_call (call);
  if (!node)
    return;

  twitter->priv->user_id = g_strdup (rest_xml_node_find (node, "id")->content);
  twitter->priv->image_url = g_strdup (rest_xml_node_find (node, "profile_image_url")->content);

  rest_xml_node_unref (node);

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));

  if (twitter->priv->running)
    get_status_updates (twitter);
}

static void
start (SwService *service)
{
  SwServiceTwitter *twitter = (SwServiceTwitter*)service;

  twitter->priv->running = TRUE;
}

static void
refresh (SwService *service)
{
  SwServiceTwitter *twitter = (SwServiceTwitter*)service;
  SwServiceTwitterPrivate *priv = twitter->priv;

  if (priv->running && priv->username && priv->password && priv->proxy) {
    get_status_updates (twitter);
  }
}

static void
avatar_downloaded_cb (const gchar *uri,
                       gchar       *local_path,
                       gpointer     userdata)
{
  SwService *service = SW_SERVICE (userdata);

  sw_service_emit_avatar_retrieved (service, local_path);
  g_free (local_path);
}

static void
request_avatar (SwService *service)
{
  SwServiceTwitterPrivate *priv = GET_PRIVATE (service);

  if (priv->image_url) {
    sw_web_download_image_async (priv->image_url,
                                     avatar_downloaded_cb,
                                     service);
  }
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwServiceTwitter *twitter = (SwServiceTwitter *)user_data;
  SwServiceTwitterPrivate *priv = twitter->priv;

  SW_DEBUG (TWITTER, "Online: %s", online ? "yes" : "no");

  if (online) {
    if (priv->username && priv->password) {
      char *url;
      char *escaped_user;
      char *escaped_password;
      RestProxyCall *call;

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

      SW_DEBUG (TWITTER, "Verifying credentials");
      call = rest_proxy_new_call (priv->proxy);
      rest_proxy_call_set_function (call, "account/verify_credentials.xml");
      rest_proxy_call_async (call, verify_cb, (GObject*)twitter, NULL, NULL);
    } else {
      sw_service_emit_refreshed ((SwService *)twitter, NULL);
    }
  } else {
    if (priv->proxy) {
      g_object_unref (priv->proxy);
      priv->proxy = NULL;
    }
    g_free (priv->user_id);
    priv->user_id = NULL;

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
sw_service_twitter_constructed (GObject *object)
{
  SwServiceTwitter *twitter = SW_SERVICE_TWITTER (object);
  SwServiceTwitterPrivate *priv;

  priv = twitter->priv = GET_PRIVATE (twitter);

  if (sw_service_get_param ((SwService *)twitter, "own")) {
    priv->type = OWN;
  } else if (sw_service_get_param ((SwService *)twitter, "friends")){
    priv->type = FRIENDS;
  } else {
    priv->type = BOTH;
  }

  priv->twitpic_re = g_regex_new ("http://twitpic.com/([A-Za-z0-9]+)", 0, 0, NULL);
  g_assert (priv->twitpic_re);

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

  sw_online_add_notify (online_notify, twitter);
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

  if (priv->twitpic_re) {
    g_regex_unref (priv->twitpic_re);
    priv->twitpic_re = NULL;
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

  object_class->constructed = sw_service_twitter_constructed;
  object_class->dispose = sw_service_twitter_dispose;
  object_class->finalize = sw_service_twitter_finalize;

  service_class->get_name = sw_service_twitter_get_name;
  service_class->start = start;
  service_class->refresh = refresh;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->request_avatar = request_avatar;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_twitter_init (SwServiceTwitter *self)
{
  SW_DEBUG (TWITTER, "new instance");
  self->priv = GET_PRIVATE (self);
}

/* Query interface */

static void
_twitter_query_open_view (SwQueryIface      *self,
                          GHashTable            *params,
                          DBusGMethodInvocation *context)
{
  SwServiceTwitterPrivate *priv = GET_PRIVATE (self);
  SwItemView *item_view;
  const gchar *object_path;

  item_view = g_object_new (SW_TYPE_TWITTER_ITEM_VIEW,
                            "proxy", priv->proxy,
                            "service", self,
                            NULL);

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

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_method (call, "POST");
  rest_proxy_call_set_function (call, "statuses/update.xml");

  rest_proxy_call_add_params (call,
                              "status", msg,
                              NULL);

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
      rest_proxy_call_add_params (call,
                                  "in_reply_to_status_id", twitter_reply_to,
                                  NULL);
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

