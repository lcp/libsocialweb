/*
 * libsocialweb - social data store
 * Copyright (C) 2010 Intel Corporation.
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
#include <libsocialweb/sw-call-list.h>

#include <glib/gi18n.h>

#include "vimeo-item-view.h"
#include "vimeo.h"

G_DEFINE_TYPE (SwVimeoItemView, sw_vimeo_item_view, SW_TYPE_ITEM_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_VIMEO_ITEM_VIEW, SwVimeoItemViewPrivate))

typedef struct _SwVimeoItemViewPrivate SwVimeoItemViewPrivate;

struct _SwVimeoItemViewPrivate {
  guint timeout_id;
  GHashTable *params;
  gchar *query;
  RestProxy *proxy;

  SwCallList *calls;
  SwSet *set;
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

static void
sw_vimeo_item_view_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  SwVimeoItemViewPrivate *priv = GET_PRIVATE (object);

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
sw_vimeo_item_view_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  SwVimeoItemViewPrivate *priv = GET_PRIVATE (object);

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
sw_vimeo_item_view_dispose (GObject *object)
{
  SwItemView *item_view = SW_ITEM_VIEW (object);
  SwVimeoItemViewPrivate *priv = GET_PRIVATE (object);

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

  if (priv->calls)
  {
    sw_call_list_free (priv->calls);
    priv->calls = NULL;
  }

  g_signal_handlers_disconnect_by_func (sw_item_view_get_service (item_view),
                                      _service_item_hidden_cb,
                                      item_view);

  G_OBJECT_CLASS (sw_vimeo_item_view_parent_class)->dispose (object);
}

static void
sw_vimeo_item_view_finalize (GObject *object)
{
  SwVimeoItemViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->query);
  g_hash_table_unref (priv->params);

  G_OBJECT_CLASS (sw_vimeo_item_view_parent_class)->finalize (object);
}

static RestXmlNode *
node_from_call (RestProxyCall *call)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *node = NULL;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_message (G_STRLOC ": error from Vimeo: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  node = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  /* No content, or wrong content */
  if (node == NULL) {
    g_message (G_STRLOC ": cannot make Vimeo call: %s", rest_proxy_call_get_payload (call));
  }

  g_object_unref (call);

  return node;
}

static void
_update_if_done (SwVimeoItemView *item_view)
{
  SwVimeoItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (sw_call_list_is_empty (priv->calls))
  {
    SwService *service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));

    SW_DEBUG (VIMEO, "Call set is empty, emitting refreshed signal");
    sw_item_view_set_from_set ((SwItemView *)item_view, priv->set);

    /* Save the results of this set to the cache */
    sw_cache_save (service,
                   priv->query,
                   priv->params,
                   priv->set);

    sw_set_empty (priv->set);
  } else {
    SW_DEBUG (VIMEO, "Call set is not empty, still more work to do.");
  }
}

static char *
make_date (const char *s)
{
  struct tm tm;
  strptime (s, "%Y-%m-%d %T", &tm);
  return sw_time_t_to_string (timegm (&tm));
}

static SwItem *
make_item (SwVimeoItemView *item_view,
           SwService        *service,
           RestXmlNode      *video_n)
{
  SwItem *item;

  item = sw_item_new ();
  sw_item_set_service (item, service);

  sw_item_put (item, "id", rest_xml_node_find (video_n, "url")->content);
  sw_item_put (item, "url", rest_xml_node_find (video_n, "url")->content);
  sw_item_put (item, "title", rest_xml_node_find (video_n, "title")->content);
  sw_item_put (item, "author", rest_xml_node_find (video_n, "user_name")->content);

  sw_item_take (item, "date", make_date (rest_xml_node_find (video_n, "upload_date")->content));

  sw_item_request_image_fetch (item, FALSE, "thumbnail", rest_xml_node_find (video_n, "thumbnail_medium")->content);
  sw_item_request_image_fetch (item, FALSE, "authoricon", rest_xml_node_find (video_n, "user_portrait_medium")->content);

  return item;
}

static void
_got_videos_cb (RestProxyCall *call,
                const GError  *error,
                GObject       *weak_object,
                gpointer       user_data)
{
  SwVimeoItemView *item_view = SW_VIMEO_ITEM_VIEW (weak_object);
  SwVimeoItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwService *service;
  RestXmlNode *root, *video_n;

  sw_call_list_remove (priv->calls, call);

  if (error) {
    g_message (G_STRLOC ": error from Vimeo: %s", error->message);
    /* TODO: cleanup or something */
    return;
  }

  SW_DEBUG (VIMEO, "Got videos");

  root = node_from_call (call);
  if (!root)
    return;

  SW_DEBUG (VIMEO, "Parsed videos");

  service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));

  for (video_n = rest_xml_node_find (root, "video"); video_n; video_n = video_n->next) {
    SwItem *item;

    /* Vimeo is stupid and returns an empty <video> element if there are no
       videos, so check for this and skip it */
    if (rest_xml_node_find (video_n, "url") == NULL)
      continue;

    item = make_item (item_view, service, video_n);

    if (!sw_service_is_uid_banned (service, sw_item_get (item, "id"))) {
      sw_set_add (priv->set, (GObject *)item);
    }

    g_object_unref (item);
  }

  rest_xml_node_unref (root);

  _update_if_done (item_view);
}

static void
_get_status_updates (SwVimeoItemView *item_view)
{
  SwVimeoItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestProxyCall *call;

  sw_call_list_cancel_all (priv->calls);
  sw_set_empty (priv->set);

  SW_DEBUG (VIMEO, "Fetching videos");
  call = rest_proxy_new_call (priv->proxy);
  sw_call_list_add (priv->calls, call);

  if (g_str_equal (priv->query, "feed"))
    rest_proxy_call_set_function (call, "subscriptions.xml");
  else if (g_str_equal (priv->query, "own"))
    rest_proxy_call_set_function (call, "videos.xml");
  else
    g_assert_not_reached ();

  rest_proxy_call_async (call,
                         _got_videos_cb,
                         (GObject *)item_view,
                         NULL,
                         NULL);
}


static gboolean
_update_timeout_cb (gpointer data)
{
  SwVimeoItemView *item_view = SW_VIMEO_ITEM_VIEW (data);

  _get_status_updates (item_view);

  return TRUE;
}

static void
_load_from_cache (SwVimeoItemView *item_view)
{
  SwVimeoItemViewPrivate *priv = GET_PRIVATE (item_view);
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
vimeo_item_view_start (SwItemView *item_view)
{
  SwVimeoItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    SW_DEBUG (VIMEO, G_STRLOC ": Setting up the timeout");
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              item_view);

    _load_from_cache ((SwVimeoItemView *)item_view);
    _get_status_updates ((SwVimeoItemView *)item_view);
  }
}

static void
vimeo_item_view_stop (SwItemView *item_view)
{
  SwVimeoItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (!priv->timeout_id)
  {
    g_warning (G_STRLOC ": View not running");
  } else {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }
}

static void
vimeo_item_view_refresh (SwItemView *item_view)
{
  _get_status_updates ((SwVimeoItemView *)item_view);
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
  set = sw_set_new ();
  sw_item_view_set_from_set (SW_ITEM_VIEW (item_view),
                             set);
  sw_set_unref (set);

  /* And drop the cache */
  sw_cache_drop_all (service);
}

static void
sw_vimeo_item_view_constructed (GObject *object)
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

  if (G_OBJECT_CLASS (sw_vimeo_item_view_parent_class)->constructed)
    G_OBJECT_CLASS (sw_vimeo_item_view_parent_class)->constructed (object);
}

static void
sw_vimeo_item_view_class_init (SwVimeoItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwItemViewClass *item_view_class = SW_ITEM_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwVimeoItemViewPrivate));

  object_class->get_property = sw_vimeo_item_view_get_property;
  object_class->set_property = sw_vimeo_item_view_set_property;
  object_class->dispose = sw_vimeo_item_view_dispose;
  object_class->finalize = sw_vimeo_item_view_finalize;
  object_class->constructed = sw_vimeo_item_view_constructed;

  item_view_class->start = vimeo_item_view_start;
  item_view_class->stop = vimeo_item_view_stop;
  item_view_class->refresh = vimeo_item_view_refresh;

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
sw_vimeo_item_view_init (SwVimeoItemView *self)
{
  SwVimeoItemViewPrivate *priv = GET_PRIVATE (self);

  priv->calls = sw_call_list_new ();
  priv->set = sw_set_new ();
}
