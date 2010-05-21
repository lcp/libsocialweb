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

#include <stdlib.h>
#include <gio/gio.h>
#include "flickr.h"
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <rest-extras/flickr-proxy.h>
#include <rest/rest-xml-parser.h>

static void initable_iface_init (gpointer g_iface, gpointer iface_data);
G_DEFINE_TYPE_WITH_CODE (SwServiceFlickr, sw_service_flickr, SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_FLICKR, SwServiceFlickrPrivate))

struct _SwServiceFlickrPrivate {
  gboolean running;
  RestProxy *proxy;

  /* Used when running a refresh */
  SwSet *set;
  gboolean refreshing;
};

static GList *service_list = NULL;

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

/* TODO: pass in size enum */
static char *
construct_photo_url (RestXmlNode *node)
{
  if (!check_attrs (node, "farm", "server", "id", "secret", NULL))
    return NULL;

  return g_strdup_printf ("http://farm%s.static.flickr.com/%s/%s_%s_m.jpg",
                          rest_xml_node_get_attr (node, "farm"),
                          rest_xml_node_get_attr (node, "server"),
                          rest_xml_node_get_attr (node, "id"),
                          rest_xml_node_get_attr (node, "secret"));
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

typedef struct {
  SwService *service;
  SwSet *set;
  /* Set of URL to list of SwItems waiting for that URL */
  GHashTable *pending;
  gboolean running;
} RefreshData;

typedef struct {
  SwServiceFlickr *service;
  const char *key;
} ImageData;

static void
refresh_done (SwServiceFlickr *service)
{
  sw_service_emit_refreshed ((SwService*)service, service->priv->set);
  service->priv->refreshing = FALSE;
  sw_set_empty (service->priv->set);
  /* TODO: more cleanup? */
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

SwItem *
_flickr_item_from_from_photo_node (SwServiceFlickr *service,
                                   RestXmlNode     *node)
{
  char *url;
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

  date = atoi (rest_xml_node_get_attr (node, "dateupload"));
  sw_item_take (item, "date", sw_time_t_to_string (date));

  url = construct_photo_url (node);
  sw_item_request_image_fetch (item, TRUE, "thumbnail", url);
  g_free (url);

  url = construct_buddy_icon_url (node);
  sw_item_request_image_fetch (item, FALSE, "authoricon", url);
  g_free (url);

  extract_location (node, item);

  return item;
}


static void
flickr_callback (RestProxyCall *call,
                 const GError  *error,
                 GObject       *weak_object,
                 gpointer       user_data)
{
  SwServiceFlickr *service = SW_SERVICE_FLICKR (weak_object);
  RestXmlParser *parser;
  RestXmlNode *root, *node;

  if (error) {
    g_warning ("Cannot get Flickr photos: %s", error->message);
    return;
  }

  parser = rest_xml_parser_new ();
  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));
  g_object_unref (call);

  node = rest_xml_node_find (root, "rsp");
  /* TODO: check for failure */

  node = rest_xml_node_find (root, "photos");
  for (node = rest_xml_node_find (node, "photo"); node; node = node->next) {
    SwItem *item;
    item = _flickr_item_from_from_photo_node (service, node);
    sw_set_add (service->priv->set, G_OBJECT (item));
    g_object_unref (item);
  }

  rest_xml_node_unref (root);
  g_object_unref (parser);

  refresh_done (service);
}

static void
get_photos (SwServiceFlickr *flickr)
{
  RestProxyCall *call;
  GError *error = NULL;

  call = rest_proxy_new_call (flickr->priv->proxy);
  rest_proxy_call_set_function (call, "flickr.photos.getContactsPhotos");
  rest_proxy_call_add_param (call, "extras", "date_upload,icon_server,geo");
  rest_proxy_call_add_param (call, "count", "50");

  if (!rest_proxy_call_async (call, flickr_callback, (GObject*)flickr, NULL, &error)) {
    g_warning ("Cannot get photos: %s", error->message);
    g_error_free (error);
  }
}

static void
got_tokens_cb (RestProxy *proxy,
               gboolean   authorised,
               gpointer   user_data)
{
  SwServiceFlickr *flickr = SW_SERVICE_FLICKR (user_data);

  if (authorised) {
    /* TODO: this assumes that the tokens are valid. we should call checkToken
       and re-auth if required. */
    get_photos (flickr);
  } else {
    sw_service_emit_refreshed ((SwService *)flickr, NULL);
  }

  /* Drop reference we took for callback */
  g_object_unref (flickr);
}

static void
refresh (SwService *service)
{
  SwServiceFlickr *flickr = (SwServiceFlickr*)service;

  if (!flickr->priv->running) {
    return;
  }

  if (flickr->priv->refreshing) {
    /* We're currently refreshing, ignore this latest request */
    return;
  }

  sw_keyfob_flickr ((FlickrProxy*)flickr->priv->proxy, 
                    got_tokens_cb,
                    g_object_ref (service)); /* ref gets dropped in cb */
}

static void
credentials_updated (SwService *service)
{
  SwService *service_instance;
  GList* node;

  for (node = service_list; node; node = g_list_next (node)){
    service_instance = SW_SERVICE (node->data);

    sw_service_emit_user_changed (service_instance);

    /* TODO: This works because we re-auth on every refresh, which is bad */
    refresh (service_instance);
  }
}

static void
start (SwService *service)
{
  SwServiceFlickr *flickr = (SwServiceFlickr*)service;

  flickr->priv->running = TRUE;
}

static const char *
sw_service_flickr_get_name (SwService *service)
{
  return "flickr";
}

static void
sw_service_flickr_dispose (GObject *object)
{
  SwServiceFlickrPrivate *priv = SW_SERVICE_FLICKR (object)->priv;

  service_list = g_list_remove (service_list, object);

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (sw_service_flickr_parent_class)->dispose (object);
}

static gboolean
sw_service_flickr_initable (GInitable    *initable,
                         GCancellable *cancellable,
                         GError      **error)
{
  SwServiceFlickr *flickr = SW_SERVICE_FLICKR (initable);
  SwServiceFlickrPrivate *priv = flickr->priv;
  const char *key = NULL, *secret = NULL;

  sw_keystore_get_key_secret ("flickr", &key, &secret);
  if (key == NULL || secret == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }

  if (priv->proxy)
    return TRUE;

  priv->proxy = flickr_proxy_new (key, secret);

  priv->set = sw_item_set_new ();
  priv->refreshing = FALSE;

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface,
                     gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_flickr_initable;
}

static void
sw_service_flickr_class_init (SwServiceFlickrClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceFlickrPrivate));

  object_class->dispose = sw_service_flickr_dispose;

  service_class->get_name = sw_service_flickr_get_name;
  service_class->start = start;
  service_class->refresh = refresh;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_flickr_init (SwServiceFlickr *self)
{
  self->priv = GET_PRIVATE (self);
  service_list = g_list_prepend (service_list, self);
}
