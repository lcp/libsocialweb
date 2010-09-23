/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 *
 * Author: Rob Bradford <rob@linux.intel.com>
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
#include <libsocialweb/sw-utils.h>

#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-cache.h>

#include "twitter-item-view.h"

G_DEFINE_TYPE (SwTwitterItemView,
               sw_twitter_item_view,
               SW_TYPE_ITEM_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_TWITTER_ITEM_VIEW, SwTwitterItemViewPrivate))

typedef struct _SwTwitterItemViewPrivate SwTwitterItemViewPrivate;

struct _SwTwitterItemViewPrivate {
  RestProxy *proxy;
  GRegex *twitpic_re;
  guint timeout_id;
  GHashTable *params;
  gchar *query;
};

enum
{
  PROP_0,
  PROP_PROXY,
  PROP_PARAMS,
  PROP_QUERY
};

#define UPDATE_TIMEOUT 5 * 60

static void _service_item_hidden_cb (SwService   *service,
                                     const gchar *uid,
                                     SwItemView  *item_view);

static void _service_user_changed_cb (SwService  *service,
                                      SwItemView *item_view);
static void _service_capabilities_changed_cb (SwService    *service,
                                              const gchar **caps,
                                              SwItemView   *item_view);

static void
sw_twitter_item_view_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  SwTwitterItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      g_value_set_object (value, priv->proxy);
      break;
    case PROP_PARAMS:
      g_value_set_boxed (value, priv->params);
      break;
    case PROP_QUERY:
      g_value_set_string (value, priv->query);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_twitter_item_view_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  SwTwitterItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      if (priv->proxy)
      {
        g_object_unref (priv->proxy);
      }
      priv->proxy = g_value_dup_object (value);
      break;
    case PROP_PARAMS:
      priv->params = g_value_dup_boxed (value);
      break;
    case PROP_QUERY:
      priv->query = g_value_dup_string (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_twitter_item_view_dispose (GObject *object)
{
  SwItemView *item_view = SW_ITEM_VIEW (object);
  SwTwitterItemViewPrivate *priv = GET_PRIVATE (object);

  if (priv->proxy)
  {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->timeout_id)
  {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }

  g_signal_handlers_disconnect_by_func (sw_item_view_get_service (item_view),
                                        _service_item_hidden_cb,
                                        item_view);
  g_signal_handlers_disconnect_by_func (sw_item_view_get_service (item_view),
                                        _service_user_changed_cb,
                                        item_view);
  g_signal_handlers_disconnect_by_func (sw_item_view_get_service (item_view),
                                        _service_capabilities_changed_cb,
                                        item_view);

  G_OBJECT_CLASS (sw_twitter_item_view_parent_class)->dispose (object);
}

static void
sw_twitter_item_view_finalize (GObject *object)
{
  SwTwitterItemViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->query);
  g_regex_unref (priv->twitpic_re);
  g_hash_table_unref (priv->params);

  G_OBJECT_CLASS (sw_twitter_item_view_parent_class)->finalize (object);
}

static char *
_make_date (const char *s)
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
_cleanup_twitpic (char *string)
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
_make_item (SwTwitterItemView *item_view,
            RestXmlNode           *node)
{
  SwTwitterItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestXmlNode *u_node, *n, *place_n;
  const char *post_id, *user_id, *user_name, *date, *content;
  char *url;
  GMatchInfo *match_info;
  SwItem *item;

  u_node = rest_xml_node_find (node, "user");

  user_id = rest_xml_node_find (u_node, "screen_name")->content;

  item = sw_item_new ();

  post_id = rest_xml_node_find (node, "id")->content;
  sw_item_put (item, "authorid", user_id);

  url = g_strdup_printf ("http://twitter.com/%s/statuses/%s", user_id, post_id);
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
    sw_item_request_image_fetch (item, TRUE, "thumbnail", url);
    g_free (url);

    /* Remove the URL from the tweet and use that as the title */
    new_content = g_regex_replace (priv->twitpic_re,
                                   content, -1,
                                   0, "", 0, NULL);

    _cleanup_twitpic (new_content);

    sw_item_take (item, "title", new_content);

    /* Update the URL to point at twitpic */
    url = g_strconcat ("http://twitpic.com/", twitpic_id, NULL);
    sw_item_take (item, "url", url);

    g_free (twitpic_id);
  }

  sw_item_put (item, "content", content);

  g_match_info_free (match_info);

  date = rest_xml_node_find (node, "created_at")->content;
  sw_item_take (item, "date", _make_date (date));

  n = rest_xml_node_find (u_node, "location");
  if (n && n->content)
    sw_item_put (item, "location", n->content);

    n = rest_xml_node_find (node, "geo");

  if (n)
  {
    n = rest_xml_node_find (n, "georss:point");

    if (n && n->content)
    {
      gchar **split_str;

      split_str = g_strsplit (n->content, " ", 2);

      if (split_str[0] && split_str[1])
      {
        sw_item_put (item, "latitude", split_str[0]);
        sw_item_put (item, "longitude", split_str[1]);
      }

      g_strfreev (split_str);
    }
  }

  place_n = rest_xml_node_find (node, "place");

  if (place_n)
  {
    n = rest_xml_node_find (place_n, "name");

    if (n && n->content)
    {
      sw_item_put (item, "place_name", n->content);
    }

    n = rest_xml_node_find (place_n, "full_name");

    if (n && n->content)
    {
      sw_item_put (item, "place_full_name", n->content);
    }
  }

  n = rest_xml_node_find (u_node, "profile_image_url");
  if (n && n->content)
    sw_item_request_image_fetch (item, FALSE, "authoricon", n->content);


  return item;
}

static RestXmlNode *
_make_node_from_call (RestProxyCall *call)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *root;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_warning (G_STRLOC ": Error from Twitter: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  if (root == NULL) {
    g_warning (G_STRLOC ": Error parsing payload from Twitter: %s",
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  return root;
}


static void
_got_status_updates_cb (RestProxyCall *call,
                        const GError  *error,
                        GObject       *weak_object,
                        gpointer       userdata)
{
  SwTwitterItemView *item_view = SW_TWITTER_ITEM_VIEW (weak_object);
  SwTwitterItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestXmlNode *root, *node;
  SwSet *set;
  SwService *service;

  if (error) {
    g_warning (G_STRLOC ": Error getting Tweets: %s", error->message);
    return;
  }

  root = _make_node_from_call (call);

  if (!root)
    return;

  set = sw_item_set_new ();

  SW_DEBUG (TWITTER, "Got tweets!");

  service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));

  for (node = rest_xml_node_find (root, "status"); node; node = node->next)
  {
    SwItem *item;

    item = _make_item (item_view, node);
    sw_item_set_service (item, service);

    if (item)
    {
      if (!sw_service_is_uid_banned (service,
                                     sw_item_get (item, "id")))
      {
        sw_set_add (set, (GObject *)item);
      }

      g_object_unref (item);
    }
  }

  sw_item_view_set_from_set (SW_ITEM_VIEW (item_view),
                             set);

  /* Save the results of this set to the cache */
  sw_cache_save (service,
                 priv->query,
                 priv->params,
                 set);

  sw_set_unref (set);
  rest_xml_node_unref (root);
}

static void
_got_trending_topic_updates_cb (RestProxyCall *call,
                                const GError  *error_in,
                                GObject       *weak_object,
                                gpointer       userdata)
{
  SwTwitterItemViewPrivate *priv = GET_PRIVATE (weak_object);
  SwItemView *item_view = SW_ITEM_VIEW (weak_object);
  SwSet *set;
  JsonParser *parser;
  JsonObject *root_o;
  SwService *service;
  GError *error = NULL;

  if (error) {
    g_warning (G_STRLOC ": Error getting trending topic data: %s", error_in->message);
    return;
  }

  service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));

  set = sw_item_set_new ();
  parser = json_parser_new ();

  if (!json_parser_load_from_data (parser,
                                   rest_proxy_call_get_payload (call),
                                   rest_proxy_call_get_payload_length (call),
                                   &error))
  {
    g_warning (G_STRLOC ": error parsing json: %s", error->message);
  } else {
    JsonNode *root_n;
    JsonObject *trends_o;
    JsonArray *trends_a;
    GList *values;
    gint i;

    root_n = json_parser_get_root (parser);
    root_o = json_node_get_object (root_n);

    trends_o = json_object_get_object_member (root_o, "trends");

    /* We have to assume just the one object member */
    if (json_object_get_size (trends_o) == 1)
    {
      values = json_object_get_values (trends_o);
      trends_a = json_node_get_array ((JsonNode *)(values->data));

      for (i = 0; i < json_array_get_length (trends_a); i++)
      {
        JsonObject *trend_o;
        SwItem *item;

        item = sw_item_new ();
        sw_item_set_service (item, service);

        trend_o = json_array_get_object_element (trends_a, i);

        sw_item_take (item, "date", sw_time_t_to_string (time(NULL)));
        sw_item_put (item, "id", json_object_get_string_member (trend_o,
                                                                "name"));
        sw_item_put (item, "content", json_object_get_string_member (trend_o,
                                                                     "name"));

        sw_set_add (set, (GObject *)item);
        g_object_unref (item);
      }
      g_list_free (values);
    }
  }


  sw_item_view_set_from_set (SW_ITEM_VIEW (item_view),
                             set);

  /* Save the results of this set to the cache */
  sw_cache_save (service,
                 priv->query,
                 priv->params,
                 set);

  sw_set_unref (set);
  g_object_unref (parser);
}

static void
_get_status_updates (SwTwitterItemView *item_view)
{
  SwTwitterItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestProxyCall *call;

  call = rest_proxy_new_call (priv->proxy);

  if (g_str_equal (priv->query, "own"))
    rest_proxy_call_set_function (call, "statuses/user_timeline.xml");
  else if (g_str_equal (priv->query, "x-twitter-mentions"))
    rest_proxy_call_set_function (call, "statuses/mentions.xml");
  else if (g_str_equal (priv->query, "feed") ||
           g_str_equal (priv->query, "friends-only"))
    rest_proxy_call_set_function (call, "statuses/friends_timeline.xml");
  else if (g_str_equal (priv->query, "x-twitter-trending-topics"))
    rest_proxy_call_set_function (call, "1/trends/current.json");
  else
    g_error (G_STRLOC ": Unexpected query '%s'", priv->query);

  if (g_str_equal (priv->query, "x-twitter-trending-topics"))
  {
    rest_proxy_call_async (call,
                           _got_trending_topic_updates_cb,
                           (GObject*)item_view,
                           NULL,
                           NULL);
  } else {
    rest_proxy_call_async (call,
                           _got_status_updates_cb,
                           (GObject*)item_view,
                           NULL,
                           NULL);
  }
  g_object_unref (call);
}

static gboolean
_update_timeout_cb (gpointer data)
{
  SwTwitterItemView *item_view = SW_TWITTER_ITEM_VIEW (data);

  _get_status_updates (item_view);

  return TRUE;
}

static void
_load_from_cache (SwTwitterItemView *item_view)
{
  SwTwitterItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwSet *set;

  set = sw_cache_load (sw_item_view_get_service (SW_ITEM_VIEW (item_view)),
                       priv->query,
                       priv->params);

  if (set)
  {
    sw_item_view_set_from_set (SW_ITEM_VIEW (item_view),
                               set);
    sw_set_unref (set);
  }
}

static void
twitter_item_view_start (SwItemView *item_view)
{
  SwTwitterItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    SW_DEBUG (TWITTER, G_STRLOC ": Setting up the timeout");
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              item_view);

    _load_from_cache ((SwTwitterItemView *)item_view);
    _get_status_updates ((SwTwitterItemView *)item_view);
  }
}

static void
twitter_item_view_stop (SwItemView *item_view)
{
  SwTwitterItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (!priv->timeout_id)
  {
    g_warning (G_STRLOC ": View not running");
  } else {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }
}

static void
twitter_item_view_refresh (SwItemView *item_view)
{
  _get_status_updates ((SwTwitterItemView *)item_view);
}

static void
_service_item_hidden_cb (SwService   *service,
                         const gchar *uid,
                         SwItemView  *item_view)
{
  sw_item_view_remove_by_uid (item_view, uid);
}

static void
_service_user_changed_cb (SwService  *service,
                          SwItemView *item_view)
{
  SwSet *set;

  /* We need to empty the set */
  set = sw_item_set_new ();
  sw_item_view_set_from_set (SW_ITEM_VIEW (item_view),
                             set);
  sw_set_unref (set);

  /* And drop the cache */
  sw_cache_drop_all (service);
}

static void
_service_capabilities_changed_cb (SwService    *service,
                                  const gchar **caps,
                                  SwItemView   *item_view)
{
  if (sw_service_has_cap (caps, CREDENTIALS_VALID))
  {
    twitter_item_view_refresh (item_view);
  }
}

static void
sw_twitter_item_view_constructed (GObject *object)
{
  SwItemView *item_view = SW_ITEM_VIEW (object);

  g_signal_connect (sw_item_view_get_service (item_view),
                    "item-hidden",
                    (GCallback)_service_item_hidden_cb,
                    item_view);

  g_signal_connect (sw_item_view_get_service (item_view),
                    "user-changed",
                    (GCallback)_service_user_changed_cb,
                    item_view);

  g_signal_connect (sw_item_view_get_service (item_view),
                    "capabilities-changed",
                    (GCallback)_service_capabilities_changed_cb,
                    item_view);

  if (G_OBJECT_CLASS (sw_twitter_item_view_parent_class)->constructed)
    G_OBJECT_CLASS (sw_twitter_item_view_parent_class)->constructed (object);
}

static void
sw_twitter_item_view_class_init (SwTwitterItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwItemViewClass *item_view_class = SW_ITEM_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwTwitterItemViewPrivate));

  object_class->get_property = sw_twitter_item_view_get_property;
  object_class->set_property = sw_twitter_item_view_set_property;
  object_class->dispose = sw_twitter_item_view_dispose;
  object_class->finalize = sw_twitter_item_view_finalize;
  object_class->constructed = sw_twitter_item_view_constructed;

  item_view_class->start = twitter_item_view_start;
  item_view_class->stop = twitter_item_view_stop;
  item_view_class->refresh = twitter_item_view_refresh;

  pspec = g_param_spec_object ("proxy",
                               "proxy",
                               "proxy",
                               REST_TYPE_PROXY,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PROXY, pspec);


  pspec = g_param_spec_string ("query",
                               "query",
                               "query",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_QUERY, pspec);


  pspec = g_param_spec_boxed ("params",
                              "params",
                              "params",
                              G_TYPE_HASH_TABLE,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PARAMS, pspec);
}

static void
sw_twitter_item_view_init (SwTwitterItemView *self)
{
  SwTwitterItemViewPrivate *priv = GET_PRIVATE (self);

  priv->twitpic_re = g_regex_new ("http://twitpic.com/([A-Za-z0-9]+)",
                                  0,
                                  0,
                                  NULL);
}
