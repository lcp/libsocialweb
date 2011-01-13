/*
 * libsocialweb Youtube service support
 *
 * Copyright (C) 2010 Novell, Inc.
 *
 * Author: Gary Ching-Pang Lin <glin@novell.com>
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

#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-cache.h>

#include <glib/gi18n.h>

#include "youtube-item-view.h"
#include "youtube.h"

G_DEFINE_TYPE (SwYoutubeItemView, sw_youtube_item_view, SW_TYPE_ITEM_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_YOUTUBE_ITEM_VIEW, SwYoutubeItemViewPrivate))

typedef struct _SwYoutubeItemViewPrivate SwYoutubeItemViewPrivate;

struct _SwYoutubeItemViewPrivate {
  guint timeout_id;
  GHashTable *params;
  gchar *query;
  RestProxy *proxy;
  gchar *developer_key;

  SwSet *set;
  GHashTable *thumb_map;
};

enum
{
  PROP_0,
  PROP_PROXY,
  PROP_PARAMS,
  PROP_QUERY,
  PROP_DEVKEY
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
sw_youtube_item_view_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  SwYoutubeItemViewPrivate *priv = GET_PRIVATE (object);

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
    case PROP_DEVKEY:
      g_value_set_string (value, priv->developer_key);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_youtube_item_view_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  SwYoutubeItemViewPrivate *priv = GET_PRIVATE (object);

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
    case PROP_DEVKEY:
      priv->developer_key = g_value_dup_string (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_youtube_item_view_dispose (GObject *object)
{
  SwItemView *item_view = SW_ITEM_VIEW (object);
  SwYoutubeItemViewPrivate *priv = GET_PRIVATE (object);

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

  G_OBJECT_CLASS (sw_youtube_item_view_parent_class)->dispose (object);
}

static void
sw_youtube_item_view_finalize (GObject *object)
{
  SwYoutubeItemViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->query);
  g_hash_table_unref (priv->params);
  g_hash_table_unref (priv->thumb_map);

  G_OBJECT_CLASS (sw_youtube_item_view_parent_class)->finalize (object);
}

static RestXmlNode *
xml_node_from_call (RestProxyCall *call,
                    const char    *name)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *root;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_message ("Error from %s: %s (%d)",
               name,
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  if (root == NULL) {
    g_message ("Error from %s: %s",
               name,
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  /* Exception handling */
  if (strcmp (name, "Youtube") == 0) {
    if (strcmp (root->name, "error_response") == 0) {
      RestXmlNode *node;
      node = rest_xml_node_find (root, "error_msg");
      g_message ("Error response from Youtube: %s\n", node->content);
      rest_xml_node_unref (root);
      return NULL;
    }
  }

  return root;
}

/*
 * For a given parent @node, get the child node called @name and return a copy
 * of the content, or NULL. If the content is the empty string, NULL is
 * returned.
 */
static char *
xml_get_child_node_value (RestXmlNode *node,
                          const char  *name)
{
  RestXmlNode *subnode;

  g_assert (node);
  g_assert (name);

  subnode = rest_xml_node_find (node, name);
  if (!subnode)
    return NULL;

  if (subnode->content && subnode->content[0])
    return g_strdup (subnode->content);
  else
    return NULL;
}

static char *
get_author_icon_url (SwYoutubeItemView *youtube, const char *author)
{
  SwYoutubeItemViewPrivate *priv = GET_PRIVATE (youtube);
  RestProxyCall *call;
  RestXmlNode *root, *node;
  char *function, *url;

  url = g_hash_table_lookup (priv->thumb_map, author);
  if (url)
    return g_strdup (url);

  call = rest_proxy_new_call (priv->proxy);
  function = g_strdup_printf ("users/%s", author);
  rest_proxy_call_set_function (call, function);
  rest_proxy_call_sync (call, NULL);

  root = xml_node_from_call (call, "Youtube");
  if (!root)
    return NULL;

  node = rest_xml_node_find (root, "media:thumbnail");
  if (!node)
    return NULL;

  url = g_strdup (rest_xml_node_get_attr (node, "url"));

  g_free (function);

  if (url)
    g_hash_table_insert(priv->thumb_map, (gpointer)author, (gpointer)g_strdup (url));

  return url;
}

static char *
get_utc_date (const char *s)
{
  struct tm tm;

  if (s == NULL)
    return NULL;

  strptime (s, "%Y-%m-%dT%T", &tm);

  return sw_time_t_to_string (mktime (&tm));
}

static SwItem *
make_item (SwYoutubeItemView *item_view,
           SwService        *service,
           RestXmlNode      *node)
{
  SwItem *item;
  char *author, *date, *url;
  RestXmlNode *subnode, *thumb_node;

  item = sw_item_new ();
  sw_item_set_service (item, service);
  /*
  <rss>
    <channel>
      <item>
        <guid isPermaLink="false">http://gdata.youtube.com/feeds/api/videos/<videoid></guid>
        <atom:updated>2010-02-13T06:17:32.000Z</atom:updated>
        <title>Video Title</title>
        <author>Author Name</author>
        <link>http://www.youtube.com/watch?v=<videoid>&amp;feature=youtube_gdata</link>
        <media:group>
          <media:thumbnail url="http://i.ytimg.com/vi/<videoid>/default.jpg" height="90" width="120" time="00:03:00.500"/>
        </media:group>
      </item>
    </channel>
  </rss>
  */

  sw_item_put (item, "id", xml_get_child_node_value (node, "guid"));

  date = xml_get_child_node_value (node, "atom:updated");
  if (date != NULL)
    sw_item_put (item, "date", get_utc_date(date));

  sw_item_put (item, "title", xml_get_child_node_value (node, "title"));
  sw_item_put (item, "url", xml_get_child_node_value (node, "link"));
  author = xml_get_child_node_value (node, "author");
  sw_item_put (item, "author", author);

  /* media:group */
  subnode = rest_xml_node_find (node, "media:group");
  if (subnode){
    thumb_node = rest_xml_node_find (subnode, "media:thumbnail");
    url = (char *) rest_xml_node_get_attr (thumb_node, "url");
    sw_item_request_image_fetch (item, TRUE, "thumbnail", url);
  }

  url = get_author_icon_url (item_view, author);
  sw_item_request_image_fetch (item, FALSE, "authoricon", url);
  g_free (url);

  return item;
}

static void
_got_videos_cb (RestProxyCall *call,
                const GError  *error,
                GObject       *weak_object,
                gpointer       user_data)
{
  SwYoutubeItemView *item_view = SW_YOUTUBE_ITEM_VIEW (weak_object);
  SwYoutubeItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwService *service;
  RestXmlNode *root, *node;

  if (error) {
    g_message (G_STRLOC ": error from Youtube: %s", error->message);
    /* TODO: cleanup or something */
    return;
  }

  root = xml_node_from_call (call, "Youtube");
  if (!root)
    return;

  node = rest_xml_node_find (root, "channel");
  if (!node)
    return;

  /* Clean up the thumbnail mapping cache */
  g_hash_table_remove_all (priv->thumb_map);

  service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));

  for (node = rest_xml_node_find (node, "item"); node; node = node->next) {
    SwItem *item;
    item = make_item (item_view, service, node);

    if (!sw_service_is_uid_banned (service, sw_item_get (item, "id"))) {
      sw_set_add (priv->set, (GObject *)item);
    }

    g_object_unref (item);
  }

  sw_item_view_set_from_set ((SwItemView *)item_view, priv->set);

  /* Save the results of this set to the cache */
  sw_cache_save (service,
                 priv->query,
                 priv->params,
                 priv->set);

  sw_set_empty (priv->set);

  rest_xml_node_unref (root);
}

static void
_get_status_updates (SwYoutubeItemView *item_view)
{
  SwYoutubeItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwService *service = sw_item_view_get_service ((SwItemView *)item_view);
  RestProxyCall *call;
  char *user_auth_header = NULL, *devkey_header = NULL;
  const char *user_auth = NULL;

  user_auth = sw_service_youtube_get_user_auth (SW_SERVICE_YOUTUBE (service));
  if (user_auth == NULL)
    return;

  sw_set_empty (priv->set);

  call = rest_proxy_new_call (priv->proxy);

  user_auth_header = g_strdup_printf ("GoogleLogin auth=%s", user_auth);
  rest_proxy_call_add_header (call, "Authorization", user_auth_header);
  devkey_header = g_strdup_printf ("key=%s", priv->developer_key);
  rest_proxy_call_add_header (call, "X-GData-Key", devkey_header);

  if (g_str_equal (priv->query, "feed"))
    rest_proxy_call_set_function (call, "users/default/newsubscriptionvideos");
  else if (g_str_equal (priv->query, "own"))
    rest_proxy_call_set_function (call, "users/default/uploads");
  else
    g_assert_not_reached ();

  rest_proxy_call_add_params (call,
                              "max-results", "10",
                              "alt", "rss",
                              NULL);

  rest_proxy_call_async (call,
                         _got_videos_cb,
                         (GObject *)item_view,
                         NULL,
                         NULL);
  g_free (user_auth_header);
  g_free (devkey_header);
}


static gboolean
_update_timeout_cb (gpointer data)
{
  SwYoutubeItemView *item_view = SW_YOUTUBE_ITEM_VIEW (data);

  _get_status_updates (item_view);

  return TRUE;
}

static void
_load_from_cache (SwYoutubeItemView *item_view)
{
  SwYoutubeItemViewPrivate *priv = GET_PRIVATE (item_view);
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
youtube_item_view_start (SwItemView *item_view)
{
  SwYoutubeItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              item_view);

    _load_from_cache ((SwYoutubeItemView *)item_view);
    _get_status_updates ((SwYoutubeItemView *)item_view);
  }
}

static void
youtube_item_view_stop (SwItemView *item_view)
{
  SwYoutubeItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (!priv->timeout_id)
  {
    g_warning (G_STRLOC ": View not running");
  } else {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }
}

static void
youtube_item_view_refresh (SwItemView *item_view)
{
  _get_status_updates ((SwYoutubeItemView *)item_view);
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
  SwYoutubeItemViewPrivate *priv = GET_PRIVATE ((SwYoutubeItemView*) item_view);

  if (sw_service_has_cap (caps, CREDENTIALS_VALID))
  {
    youtube_item_view_refresh (item_view);

    if (!priv->timeout_id)
    {
      priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                                (GSourceFunc)_update_timeout_cb,
                                                item_view);
    }
  } else {
    if (priv->timeout_id)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }
  }
}

static void
sw_youtube_item_view_constructed (GObject *object)
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

  if (G_OBJECT_CLASS (sw_youtube_item_view_parent_class)->constructed)
    G_OBJECT_CLASS (sw_youtube_item_view_parent_class)->constructed (object);
}

static void
sw_youtube_item_view_class_init (SwYoutubeItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwItemViewClass *item_view_class = SW_ITEM_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwYoutubeItemViewPrivate));

  object_class->get_property = sw_youtube_item_view_get_property;
  object_class->set_property = sw_youtube_item_view_set_property;
  object_class->dispose = sw_youtube_item_view_dispose;
  object_class->finalize = sw_youtube_item_view_finalize;
  object_class->constructed = sw_youtube_item_view_constructed;

  item_view_class->start = youtube_item_view_start;
  item_view_class->stop = youtube_item_view_stop;
  item_view_class->refresh = youtube_item_view_refresh;

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


  pspec = g_param_spec_string ("developer_key",
                               "developer_key",
                               "developer_key",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_DEVKEY, pspec);
}

static void
sw_youtube_item_view_init (SwYoutubeItemView *self)
{
  SwYoutubeItemViewPrivate *priv = GET_PRIVATE (self);

  priv->set = sw_item_set_new ();
  priv->thumb_map = g_hash_table_new(g_str_hash, g_str_equal);
}
