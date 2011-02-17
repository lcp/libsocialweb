/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2010 Intel Corporation.
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

#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb/sw-cache.h>

#include <rest-extras/flickr-proxy.h>
#include "flickr-item-view.h"
#include "flickr.h"

G_DEFINE_TYPE (SwFlickrItemView, sw_flickr_item_view, SW_TYPE_ITEM_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_FLICKR_ITEM_VIEW, SwFlickrItemViewPrivate))

typedef struct _SwFlickrItemViewPrivate SwFlickrItemViewPrivate;

struct _SwFlickrItemViewPrivate {
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
sw_flickr_item_view_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  SwFlickrItemViewPrivate *priv = GET_PRIVATE (object);

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
sw_flickr_item_view_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  SwFlickrItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
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
sw_flickr_item_view_dispose (GObject *object)
{
  SwItemView *item_view = SW_ITEM_VIEW (object);
  SwFlickrItemViewPrivate *priv = GET_PRIVATE (object);

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

  G_OBJECT_CLASS (sw_flickr_item_view_parent_class)->dispose (object);
}

static void
sw_flickr_item_view_finalize (GObject *object)
{
  SwFlickrItemViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->query);
  g_hash_table_unref (priv->params);

  G_OBJECT_CLASS (sw_flickr_item_view_parent_class)->finalize (object);
}

static gboolean
check_attrs (RestXmlNode *node, ...)
{
  va_list attrs;
  const char *name;

  g_assert (node);

  va_start (attrs, node);
  while ((name = va_arg (attrs, char*)) != NULL) {
    if (rest_xml_node_get_attr (node, name) == NULL) {
      g_warning ("XML node doesn't have required attribute %s", name);
      return FALSE;
    }
  }
  va_end (attrs);

  return TRUE;
}

static char *
construct_photo_page_url (RestXmlNode *node)
{
  if (!check_attrs (node, "owner", "id", NULL))
    return NULL;

  return g_strdup_printf ("http://www.flickr.com/photos/%s/%s/",
                          rest_xml_node_get_attr (node, "owner"),
                          rest_xml_node_get_attr (node, "id"));
}

static char *
construct_buddy_icon_url (RestXmlNode *node)
{
  if (!check_attrs (node, "iconfarm", "iconserver", "owner", NULL))
    return g_strdup ("http://www.flickr.com/images/buddyicon.jpg");

  if (atoi (rest_xml_node_get_attr (node, "iconserver")) == 0)
    return g_strdup ("http://www.flickr.com/images/buddyicon.jpg");

  return g_strdup_printf ("http://farm%s.static.flickr.com/%s/buddyicons/%s.jpg",
                          rest_xml_node_get_attr (node, "iconfarm"),
                          rest_xml_node_get_attr (node, "iconserver"),
                          rest_xml_node_get_attr (node, "owner"));
}

static void
extract_location (RestXmlNode *node, SwItem *item)
{
  const char *acc, *lon, *lat;

  g_assert (node);
  g_assert (SW_IS_ITEM (item));

  acc = rest_xml_node_get_attr (node, "accuracy");
  if (acc == NULL || atoi (acc) == 0)
    return;

  lat = rest_xml_node_get_attr (node, "latitude");
  lon = rest_xml_node_get_attr (node, "longitude");

  if (lat == NULL || lat[0] == '\0' ||
      lon == NULL || lon[0] == '\0')
    return;

  sw_item_put (item, "latitude", lat);
  sw_item_put (item, "longitude", lon);
}

static SwItem *
_flickr_item_from_from_photo_node (SwServiceFlickr *service,
                                   RestXmlNode     *node)
{
  char *url;
  const char *photo_url;
  SwItem *item;
  gint64 date;

  item = sw_item_new ();
  sw_item_set_service (item, (SwService *)service);

  url = construct_photo_page_url (node);
  sw_item_put (item, "id", url);
  sw_item_put (item, "title", rest_xml_node_get_attr (node, "title"));
  sw_item_put (item, "authorid", rest_xml_node_get_attr (node, "owner"));
  sw_item_put (item, "author", rest_xml_node_get_attr (node, "username"));
  sw_item_put (item, "url", url);
  g_free (url);

  photo_url = rest_xml_node_get_attr (node, "url_o");
  if (!photo_url)
    photo_url = rest_xml_node_get_attr (node, "url_l");
  if (!photo_url)
    photo_url = rest_xml_node_get_attr (node, "url_m");

  sw_item_put (item, "x-flickr-photo-url", photo_url);

  date = atoi (rest_xml_node_get_attr (node, "dateupload"));
  sw_item_take (item, "date", sw_time_t_to_string (date));

  photo_url = rest_xml_node_get_attr (node, "url_m");
  sw_item_request_image_fetch (item, TRUE, "thumbnail", photo_url);

  url = construct_buddy_icon_url (node);
  sw_item_request_image_fetch (item, FALSE, "authoricon", url);
  g_free (url);

  extract_location (node, item);

  return item;
}



static void
_photos_received_cb (RestProxyCall *call,
                     const GError  *error,
                     GObject       *weak_object,
                     gpointer       userdata)
{
  SwItemView *item_view = SW_ITEM_VIEW (weak_object);
  SwFlickrItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestXmlParser *parser;
  RestXmlNode *root, *node;
  SwSet *set;
  SwService *service;

  if (error) {
    g_warning ("Cannot get Flickr photos: %s", error->message);
    return;
  }

  parser = rest_xml_parser_new ();
  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));
  set = sw_item_set_new ();

  node = rest_xml_node_find (root, "photos");

  service = sw_item_view_get_service (item_view);

  for (node = rest_xml_node_find (node, "photo"); node; node = node->next) {
    SwItem *item;

    item = _flickr_item_from_from_photo_node (SW_SERVICE_FLICKR (service),
                                              node);


    if (!sw_service_is_uid_banned (service,
                                   sw_item_get (item, "id")))
    {
      sw_set_add (set, (GObject *)item);
    }

    g_object_unref (item);
  }

  rest_xml_node_unref (root);
  g_object_unref (parser);

  sw_item_view_set_from_set (item_view, set);
  sw_cache_save (service,
                 priv->query,
                 priv->params,
                 set);

  sw_set_unref (set);
}

static void
_make_request (SwFlickrItemView *item_view)
{
  SwFlickrItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestProxyCall *call;
  GError *error = NULL;

  call = rest_proxy_new_call (priv->proxy);

  if (g_str_equal (priv->query, "x-flickr-search"))
  {
    /* http://www.flickr.com/services/api/flickr.photos.search.html */
    rest_proxy_call_set_function (call, "flickr.photos.search");

    if (g_hash_table_lookup (priv->params, "text"))
      rest_proxy_call_add_param (call, "text", g_hash_table_lookup (priv->params, "text"));

    if (g_hash_table_lookup (priv->params, "tags"))
      rest_proxy_call_add_param (call, "tags", g_hash_table_lookup (priv->params, "tags"));

    if (g_hash_table_lookup (priv->params, "licenses"))
      rest_proxy_call_add_param (call,
                                 "license",
                                 g_hash_table_lookup (priv->params, "licenses"));

  } else if (g_str_equal (priv->query, "own")) {

    /* http://www.flickr.com/services/api/flickr.people.getPhotos.html */
    rest_proxy_call_set_function (call, "flickr.people.getPhotos");
    rest_proxy_call_add_param (call, "user_id", "me");

  } else if (g_str_equal (priv->query, "friends-only") || 
             g_str_equal (priv->query, "feed")) {
    /* 
     * http://www.flickr.com/services/api/flickr.photos.getContactsPhotos.html
     * */
    rest_proxy_call_set_function (call, "flickr.photos.getContactsPhotos");

    if (g_str_equal (priv->query, "friends-only"))
    {
      rest_proxy_call_add_param (call, "include_self", "0");
    } else {
      rest_proxy_call_add_param (call, "include_self", "1");
    }
  } else {
    g_error (G_STRLOC ": Unexpected query '%s", priv->query);
  }

  rest_proxy_call_add_param (call, "count", "50");
  rest_proxy_call_add_param (call, "extras",
                             "date_upload,icon_server,geo,url_m,url_l,url_o");

  if (!rest_proxy_call_async (call,
                              _photos_received_cb,
                              (GObject *)item_view,
                              NULL,
                              &error)) {
    g_warning ("Cannot get photos: %s", error->message);
    g_error_free (error);
  }

  g_object_unref (call);
}

static void
_got_tokens_cb (RestProxy *proxy,
                gboolean   authorised,
                gpointer   userdata)
{
  SwFlickrItemView *item_view = SW_FLICKR_ITEM_VIEW (userdata);

  if (authorised) 
  {
    _make_request (item_view);
  }

  /* Drop reference we took for callback */
  g_object_unref (item_view);
}

static void
_get_status_updates (SwFlickrItemView *item_view)
{
  SwFlickrItemViewPrivate *priv = GET_PRIVATE (item_view);

  sw_keyfob_flickr ((FlickrProxy *)priv->proxy,
                    _got_tokens_cb,
                    g_object_ref (item_view)); /* ref gets dropped in cb */
}

static gboolean
_update_timeout_cb (gpointer data)
{
  SwFlickrItemView *item_view = SW_FLICKR_ITEM_VIEW (data);

  _get_status_updates (item_view);

  return TRUE;
}

static void
_load_from_cache (SwFlickrItemView *item_view)
{
  SwFlickrItemViewPrivate *priv = GET_PRIVATE (item_view);
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
flickr_item_view_start (SwItemView *item_view)
{
  SwFlickrItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              item_view);
    _load_from_cache ((SwFlickrItemView *)item_view);
    _get_status_updates ((SwFlickrItemView *)item_view);
  }
}

static void
flickr_item_view_stop (SwItemView *item_view)
{
  SwFlickrItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (!priv->timeout_id)
  {
    g_warning (G_STRLOC ": View not running");
  } else {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }
}

static void
flickr_item_view_refresh (SwItemView *item_view)
{
  _get_status_updates ((SwFlickrItemView *)item_view);
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
  SwFlickrItemViewPrivate *priv = GET_PRIVATE ((SwFlickrItemView*) item_view);

  if (sw_service_has_cap (caps, CREDENTIALS_VALID))
  {
    flickr_item_view_refresh (item_view);
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
sw_flickr_item_view_constructed (GObject *object)
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

  if (G_OBJECT_CLASS (sw_flickr_item_view_parent_class)->constructed)
    G_OBJECT_CLASS (sw_flickr_item_view_parent_class)->constructed (object);
}

static void
sw_flickr_item_view_class_init (SwFlickrItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwItemViewClass *item_view_class = SW_ITEM_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwFlickrItemViewPrivate));

  object_class->get_property = sw_flickr_item_view_get_property;
  object_class->set_property = sw_flickr_item_view_set_property;
  object_class->dispose = sw_flickr_item_view_dispose;
  object_class->finalize = sw_flickr_item_view_finalize;
  object_class->constructed = sw_flickr_item_view_constructed;

  item_view_class->start = flickr_item_view_start;
  item_view_class->stop = flickr_item_view_stop;
  item_view_class->refresh = flickr_item_view_refresh;

  pspec = g_param_spec_object ("proxy",
                               "proxy",
                               "proxy",
                               REST_TYPE_PROXY,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PROXY, pspec);

  pspec = g_param_spec_boxed ("params",
                              "params",
                              "params",
                              G_TYPE_HASH_TABLE,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PARAMS, pspec);

  pspec = g_param_spec_string ("query",
                               "query",
                               "query",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_QUERY, pspec);
}

static void
sw_flickr_item_view_init (SwFlickrItemView *self)
{
}

