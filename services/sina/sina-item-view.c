/*
 * libsocialweb Sina service support
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

#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>

#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-cache.h>

#include "sina-item-view.h"

G_DEFINE_TYPE (SwSinaItemView,
               sw_sina_item_view,
               SW_TYPE_ITEM_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SINA_ITEM_VIEW, SwSinaItemViewPrivate))

typedef struct _SwSinaItemViewPrivate SwSinaItemViewPrivate;

struct _SwSinaItemViewPrivate {
  RestProxy *proxy;
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
sw_sina_item_view_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (object);

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
sw_sina_item_view_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (object);

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
sw_sina_item_view_dispose (GObject *object)
{
  SwItemView *item_view = SW_ITEM_VIEW (object);
  SwSinaItemViewPrivate *priv = GET_PRIVATE (object);

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->timeout_id) {
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

  G_OBJECT_CLASS (sw_sina_item_view_parent_class)->dispose (object);
}

static void
sw_sina_item_view_finalize (GObject *object)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (object);

  /* free private variables */
  g_free (priv->query);
  g_hash_table_unref (priv->params);

  G_OBJECT_CLASS (sw_sina_item_view_parent_class)->finalize (object);
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

static char*
make_date (const char *s)
{
  struct tm tm = {0};
  strptime (s, "%A %h %d %T %z %Y", &tm);
  return sw_time_t_to_string (mktime (&tm));
}

static void
_populate_set_from_node (SwService   *service,
                         SwSet       *set,
                         RestXmlNode *root)
{
  RestXmlNode *node;

  if (!root)
    return;

  node = rest_xml_node_find (root, "status");
  while (node) {
    SwItem *item;
    RestXmlNode *user;
    char *id, *date, *uid, *url;

    item = sw_item_new ();
    sw_item_set_service (item, service);

    user = rest_xml_node_find (node, "user");

    id = g_strconcat ("sina-",
                      xml_get_child_node_value (node, "id"),
                      NULL);
    sw_item_take (item, "id", id);

    date = xml_get_child_node_value (node, "created_at");
    sw_item_take (item, "date", make_date (date));
    g_free (date);

    sw_item_take (item,
                  "author",
                  xml_get_child_node_value (user, "screen_name"));

    url = xml_get_child_node_value (user, "profile_image_url");
    sw_item_request_image_fetch (item, FALSE, "authoricon", url);
    g_free (url);

    sw_item_take (item,
                  "content",
                  xml_get_child_node_value (node, "text"));

    uid = xml_get_child_node_value (user, "id");
    url = g_strconcat ("http://t.sina.com.cn/", uid, NULL);
    sw_item_take (item, "url", url);
    g_free (uid);

    if (!sw_service_is_uid_banned (service, sw_item_get (item, "id"))) {
      sw_set_add (set, G_OBJECT (item));
    }
    g_object_unref (item);

    /* Next node */
    node = node->next;
  }
}

static void _get_user_status_updates (SwSinaItemView *item_view, SwSet *set);

static void
_got_user_status_cb (RestProxyCall *call,
                     const GError  *error,
                     GObject       *weak_object,
                     gpointer       userdata)
{
  SwSinaItemView *item_view = SW_SINA_ITEM_VIEW (weak_object);
  SwSinaItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwSet *set = (SwSet *)userdata;
  RestXmlNode *root;
  SwService *service;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));

  root = xml_node_from_call (call, "Sina");
  _populate_set_from_node (service, set, root);
  rest_xml_node_unref (root);

  g_object_unref (call);

  sw_item_view_set_from_set (SW_ITEM_VIEW (item_view), set);

  /* Save the results of this set to the cache */
  sw_cache_save (service,
                 priv->query,
                 priv->params,
                 set);

  sw_set_unref (set);
}

static void
_got_friends_status_cb (RestProxyCall *call,
                        const GError  *error,
                        GObject       *weak_object,
                        gpointer       userdata)
{
  SwSinaItemView *item_view = SW_SINA_ITEM_VIEW (weak_object);
  SwSet *set = (SwSet *)userdata;
  RestXmlNode *root;
  SwService *service;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));

  root = xml_node_from_call (call, "sina");
  _populate_set_from_node (service, set, root);
  rest_xml_node_unref (root);

  g_object_unref (call);

  _get_user_status_updates (item_view, set);
}

static void
_get_user_status_updates (SwSinaItemView *item_view,
                          SwSet          *set)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestProxyCall *call;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "statuses/user_timeline.xml");
  rest_proxy_call_add_params(call,
                             "count", "10",
                             NULL);
  rest_proxy_call_async (call, _got_user_status_cb, (GObject*)item_view, set, NULL);
}

static void
_get_friends_status_updates (SwSinaItemView *item_view,
                             SwSet          *set)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestProxyCall *call;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "statuses/friends_timeline.xml");
  rest_proxy_call_add_params(call,
                             "count", "10",
                             NULL);
  rest_proxy_call_async (call, _got_friends_status_cb, (GObject*)item_view, set, NULL);
}

static void
_get_status_updates (SwSinaItemView *item_view)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwSet *set;

  set = sw_item_set_new ();

  if (g_str_equal (priv->query, "own"))
    _get_user_status_updates (item_view, set);
  else if (g_str_equal (priv->query, "feed"))
    _get_friends_status_updates (item_view, set);
  else
    g_error (G_STRLOC ": Unexpected query '%s'", priv->query);
}

static gboolean
_update_timeout_cb (gpointer data)
{
  SwSinaItemView *item_view = SW_SINA_ITEM_VIEW (data);

  _get_status_updates (item_view);

  return TRUE;
}

static void
_load_from_cache (SwSinaItemView *item_view)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (item_view);
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
sina_item_view_start (SwItemView *item_view)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              item_view);
    _load_from_cache ((SwSinaItemView *)item_view);
    _get_status_updates ((SwSinaItemView *)item_view);
  }
}

static void
sina_item_view_stop (SwItemView *item_view)
{
  SwSinaItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (!priv->timeout_id)
  {
    g_warning (G_STRLOC ": View not running");
  } else {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }
}

static void
sina_item_view_refresh (SwItemView *item_view)
{
  _get_status_updates ((SwSinaItemView *)item_view);
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
  SwSinaItemViewPrivate *priv = GET_PRIVATE ((SwSinaItemView*) item_view);

  if (sw_service_has_cap (caps, CREDENTIALS_VALID))
  {
    sina_item_view_refresh (item_view);

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
sw_sina_item_view_constructed (GObject *object)
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

  if (G_OBJECT_CLASS (sw_sina_item_view_parent_class)->constructed)
    G_OBJECT_CLASS (sw_sina_item_view_parent_class)->constructed (object);
}

static void
sw_sina_item_view_class_init (SwSinaItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwItemViewClass *item_view_class = SW_ITEM_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwSinaItemViewPrivate));

  object_class->get_property = sw_sina_item_view_get_property;
  object_class->set_property = sw_sina_item_view_set_property;
  object_class->dispose = sw_sina_item_view_dispose;
  object_class->finalize = sw_sina_item_view_finalize;
  object_class->constructed = sw_sina_item_view_constructed;

  item_view_class->start = sina_item_view_start;
  item_view_class->stop = sina_item_view_stop;
  item_view_class->refresh = sina_item_view_refresh;

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
sw_sina_item_view_init (SwSinaItemView *self)
{
  /* Initialize private variables */
}
