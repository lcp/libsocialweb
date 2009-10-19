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

#include <stdlib.h>
#include "mojito-service-flickr.h"
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>
#include <mojito/mojito-utils.h>
#include <mojito/mojito-web.h>
#include <mojito-keystore/mojito-keystore.h>
#include <mojito-keyfob/mojito-keyfob.h>
#include <rest-extras/flickr-proxy.h>
#include <rest/rest-xml-parser.h>

G_DEFINE_TYPE (MojitoServiceFlickr, mojito_service_flickr, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_FLICKR, MojitoServiceFlickrPrivate))

struct _MojitoServiceFlickrPrivate {
  gboolean running;
  RestProxy *proxy;

  /* Used when running a refresh */
  MojitoSet *set;
  gboolean refreshing;
};

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
  MojitoService *service;
  MojitoSet *set;
  /* Set of URL to list of MojitoItems waiting for that URL */
  GHashTable *pending;
  gboolean running;
} RefreshData;

typedef struct {
  MojitoServiceFlickr *service;
  const char *key;
} ImageData;

static void
refresh_done (MojitoServiceFlickr *service)
{
  mojito_service_emit_refreshed ((MojitoService*)service, service->priv->set);
  service->priv->refreshing = FALSE;
  mojito_set_empty (service->priv->set);
  /* TODO: more cleanup? */
}

static void
flickr_callback (RestProxyCall *call,
                 const GError  *error,
                 GObject       *weak_object,
                 gpointer       user_data)
{
  MojitoServiceFlickr *service = MOJITO_SERVICE_FLICKR (weak_object);
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
    char *url;
    MojitoItem *item;
    gint64 date;

    item = mojito_item_new ();
    mojito_item_set_service (item, (MojitoService*)service);

    url = construct_photo_page_url (node);
    mojito_item_put (item, "id", url);
    mojito_item_put (item, "title", rest_xml_node_get_attr (node, "title"));
    mojito_item_put (item, "authorid", rest_xml_node_get_attr (node, "owner"));
    mojito_item_put (item, "author", rest_xml_node_get_attr (node, "username"));
    mojito_item_put (item, "url", url);
    g_free (url);

    date = atoi (rest_xml_node_get_attr (node, "dateupload"));
    mojito_item_take (item, "date", mojito_time_t_to_string (date));

    url = construct_photo_url (node);
    mojito_item_request_image_fetch (item, "thumbnail", url);
    g_free (url);

    url = construct_buddy_icon_url (node);
    mojito_item_request_image_fetch (item, "authoricon", url);
    g_free (url);

    mojito_set_add (service->priv->set, G_OBJECT (item));
    g_object_unref (item);
  }

  rest_xml_node_unref (root);
  g_object_unref (parser);

  refresh_done (service);
}

static void
get_photos (MojitoServiceFlickr *flickr)
{
  RestProxyCall *call;
  GError *error = NULL;

  call = rest_proxy_new_call (flickr->priv->proxy);
  rest_proxy_call_set_function (call, "flickr.photos.getContactsPhotos");
  rest_proxy_call_add_param (call, "extras", "date_upload,icon_server");

  if (!rest_proxy_call_async (call, flickr_callback, (GObject*)flickr, NULL, &error)) {
    g_warning ("Cannot get photos: %s", error->message);
    g_error_free (error);
  }
}

static void
got_tokens_cb (RestProxy *proxy, gboolean authorised, gpointer user_data)
{
  MojitoServiceFlickr *flickr = MOJITO_SERVICE_FLICKR (user_data);

  if (authorised) {
    /* TODO: this assumes that the tokens are valid. we should call checkToken
       and re-auth if required. */
    get_photos (flickr);
  } else {
    mojito_service_emit_refreshed ((MojitoService *)flickr, NULL);
  }
}

static void
refresh (MojitoService *service)
{
  MojitoServiceFlickr *flickr = (MojitoServiceFlickr*)service;

  if (!flickr->priv->running) {
    return;
  }

  if (flickr->priv->refreshing) {
    /* We're currently refreshing, ignore this latest request */
    return;
  }

  mojito_keyfob_flickr ((FlickrProxy*)flickr->priv->proxy, got_tokens_cb, service);
  /* TODO: BUG: this is a async operation and before it invokes the callback the
     service object might have died. */
}

static void
start (MojitoService *service)
{
  MojitoServiceFlickr *flickr = (MojitoServiceFlickr*)service;

  flickr->priv->running = TRUE;
}

static const char *
mojito_service_flickr_get_name (MojitoService *service)
{
  return "flickr";
}

static void
mojito_service_flickr_dispose (GObject *object)
{
  MojitoServiceFlickrPrivate *priv = MOJITO_SERVICE_FLICKR (object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (mojito_service_flickr_parent_class)->dispose (object);
}

static void
mojito_service_flickr_class_init (MojitoServiceFlickrClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoServiceClass *service_class = MOJITO_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoServiceFlickrPrivate));

  object_class->dispose = mojito_service_flickr_dispose;

  service_class->get_name = mojito_service_flickr_get_name;
  service_class->start = start;
  service_class->refresh = refresh;
}

static void
mojito_service_flickr_init (MojitoServiceFlickr *self)
{
  MojitoServiceFlickrPrivate *priv;
  const char *key = NULL, *secret = NULL;

  self->priv = priv = GET_PRIVATE (self);

  mojito_keystore_get_key_secret ("flickr", &key, &secret);
  priv->proxy = flickr_proxy_new (key, secret);

  priv->set = mojito_item_set_new ();
  priv->refreshing = FALSE;
}
