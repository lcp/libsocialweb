/*
 * Mojito - social data store
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
#include <mojito/mojito-utils.h>

#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>

#include <mojito/mojito-debug.h>
#include <mojito/mojito-item.h>

#include "twitter-item-view.h"

G_DEFINE_TYPE (MojitoTwitterItemView,
               mojito_twitter_item_view,
               MOJITO_TYPE_ITEM_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_TWITTER_ITEM_VIEW, MojitoTwitterItemViewPrivate))

typedef struct _MojitoTwitterItemViewPrivate MojitoTwitterItemViewPrivate;

struct _MojitoTwitterItemViewPrivate {
  RestProxy *proxy;
  GRegex *twitpic_re;
  guint timeout_id;
};

enum
{
  PROP_0,
  PROP_PROXY
};

#define UPDATE_TIMEOUT 5 * 60

static void
mojito_twitter_item_view_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  MojitoTwitterItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      g_value_set_object (value, priv->proxy);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_twitter_item_view_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  MojitoTwitterItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      if (priv->proxy)
      {
        g_object_unref (priv->proxy);
      }
      priv->proxy = g_value_dup_object (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_twitter_item_view_dispose (GObject *object)
{
  MojitoTwitterItemViewPrivate *priv = GET_PRIVATE (object);

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

  G_OBJECT_CLASS (mojito_twitter_item_view_parent_class)->dispose (object);
}

static void
mojito_twitter_item_view_finalize (GObject *object)
{
  G_OBJECT_CLASS (mojito_twitter_item_view_parent_class)->finalize (object);
}

static char *
_make_date (const char *s)
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

static MojitoItem *
_make_item (MojitoTwitterItemView *item_view,
            RestXmlNode           *node)
{
  MojitoTwitterItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestXmlNode *u_node, *n;
  const char *post_id, *user_id, *user_name, *date, *content;
  char *url;
  GMatchInfo *match_info;
  MojitoItem *item;

  u_node = rest_xml_node_find (node, "user");

  user_id = rest_xml_node_find (u_node, "screen_name")->content;

  item = mojito_item_new ();

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
    mojito_item_request_image_fetch (item, FALSE, "thumbnail", url);
    g_free (url);

    /* Remove the URL from the tweet and use that as the title */
    new_content = g_regex_replace (priv->twitpic_re,
                                   content, -1,
                                   0, "", 0, NULL);

    _cleanup_twitpic (new_content);

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
  mojito_item_take (item, "date", _make_date (date));

  n = rest_xml_node_find (u_node, "location");
  if (n && n->content)
    mojito_item_put (item, "location", n->content);

  n = rest_xml_node_find (u_node, "profile_image_url");
  if (n && n->content)
    mojito_item_request_image_fetch (item, FALSE, "authoricon", n->content);


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
  MojitoTwitterItemView *item_view = MOJITO_TWITTER_ITEM_VIEW (weak_object);
  RestXmlNode *root, *node;
  MojitoSet *set;
  MojitoService *service;

  if (error) {
    g_warning (G_STRLOC ": Error getting Tweets: %s", error->message);
    return;
  }

  root = _make_node_from_call (call);

  if (!root)
    return;

  set = mojito_item_set_new ();

  MOJITO_DEBUG (TWITTER, "Got tweets!");

  for (node = rest_xml_node_find (root, "status"); node; node = node->next) {
    MojitoItem *item;

    item = _make_item (item_view, node);
    service = mojito_item_view_get_service (MOJITO_ITEM_VIEW (item_view));
    mojito_item_set_service (item, service);
    if (item)
      mojito_set_add (set, (GObject *)item);
  }

  mojito_item_view_set_from_set (MOJITO_ITEM_VIEW (item_view),
                                 set);
  mojito_set_unref (set);
  rest_xml_node_unref (root);
}

static void
_get_status_updates (MojitoTwitterItemView *item_view)
{
  MojitoTwitterItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestProxyCall *call;

  call = rest_proxy_new_call (priv->proxy);

  /* TODO: Use query parameters here */
  rest_proxy_call_set_function (call, "statuses/friends_timeline.xml");

  rest_proxy_call_async (call,
                         _got_status_updates_cb,
                         (GObject*)item_view,
                         NULL,
                         NULL);
}

static gboolean
_update_timeout_cb (gpointer data)
{
  MojitoTwitterItemView *item_view = MOJITO_TWITTER_ITEM_VIEW (data);

  _get_status_updates (item_view);

  return TRUE;
}

static void
twitter_item_view_start (MojitoItemView *item_view)
{
  MojitoTwitterItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    MOJITO_DEBUG (TWITTER, G_STRLOC ": Setting up the timeout");
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              item_view);
    _get_status_updates ((MojitoTwitterItemView *)item_view);
  }
}

static void
mojito_twitter_item_view_class_init (MojitoTwitterItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoItemViewClass *item_view_class = MOJITO_ITEM_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MojitoTwitterItemViewPrivate));

  object_class->get_property = mojito_twitter_item_view_get_property;
  object_class->set_property = mojito_twitter_item_view_set_property;
  object_class->dispose = mojito_twitter_item_view_dispose;
  object_class->finalize = mojito_twitter_item_view_finalize;

  item_view_class->start = twitter_item_view_start;

  pspec = g_param_spec_object ("proxy",
                               "proxy",
                               "proxy",
                               REST_TYPE_PROXY,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PROXY, pspec);
}

static void
mojito_twitter_item_view_init (MojitoTwitterItemView *self)
{
  MojitoTwitterItemViewPrivate *priv = GET_PRIVATE (self);

  priv->twitpic_re = g_regex_new ("http://twitpic.com/([A-Za-z0-9]+)",
                                  0,
                                  0,
                                  NULL);
}
