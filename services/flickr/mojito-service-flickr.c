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
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <gconf/gconf-client.h>

G_DEFINE_TYPE (MojitoServiceFlickr, mojito_service_flickr, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_FLICKR, MojitoServiceFlickrPrivate))

#define KEY_BASE "/apps/mojito/services/flickr"
#define KEY_USER KEY_BASE "/user"

struct _MojitoServiceFlickrPrivate {
  GConfClient *gconf;
  RestProxy *proxy;
  char *user_id;
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

/* TODO: this should be async blaa blaa */
static char *
get_buddy_icon (RestXmlNode *node)
{
  char *url, *filename;

  g_assert (node);

  url = construct_buddy_icon_url (node);
  filename = mojito_web_download_image (url);
  g_free (url);

  return filename;
}

/* TODO: this should be async blaa blaa */
static char *
get_thumbnail (RestXmlNode *node)
{
  char *url, *filename;

  g_assert (node);

  url = construct_photo_url (node);
  filename = mojito_web_download_image (url);
  g_free (url);

  return filename;
}

typedef struct {
  GHashTable *params;
  MojitoServiceDataFunc callback;
  gpointer user_data;
}UpdateData;

static void
flickr_callback (RestProxyCall *call,
                 GError *error,
                 GObject *weak_object,
                 gpointer user_data)
{
  MojitoServiceFlickr *service = MOJITO_SERVICE_FLICKR (weak_object);
  UpdateData *data = user_data;
  RestXmlParser *parser;
  RestXmlNode *root, *node;
  MojitoSet *set;

  if (error) {
    g_warning ("Cannot get Flickr photos: %s", error->message);
    data->callback ((MojitoService*)service, data->params, NULL, data->user_data);
    g_slice_free (UpdateData, data);
    return;
  }

  parser = rest_xml_parser_new ();
  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));
  g_object_unref (call);

  node = rest_xml_node_find (root, "rsp");
  /* TODO: check for failure */

  set = mojito_item_set_new ();

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

    mojito_item_take (item, "authoricon", get_buddy_icon (node));
    mojito_item_take (item, "thumbnail", get_thumbnail (node));

    mojito_set_add (set, G_OBJECT (item));
    g_object_unref (item);
  }

  rest_xml_node_unref (root);
  g_object_unref (parser);

  data->callback ((MojitoService*)service, data->params, set, data->user_data);

  g_slice_free (UpdateData, data);
}

static void
update (MojitoService *service, GHashTable *params, MojitoServiceDataFunc callback, gpointer user_data)
{
  MojitoServiceFlickr *flickr = (MojitoServiceFlickr*)service;
  UpdateData *data;
  RestProxyCall *call;

  if (flickr->priv->user_id == NULL) {
    callback (service, params, NULL, user_data);
    return;
  }

  data = g_slice_new (UpdateData);
  data->callback = callback;
  data->user_data = user_data;

  call = rest_proxy_new_call (flickr->priv->proxy);
  rest_proxy_call_add_params (call,
                              "method", "flickr.photos.getContactsPublicPhotos",
                              "api_key", FLICKR_APIKEY,
                              "user_id", flickr->priv->user_id,
                              "extras", "date_upload,icon_server",
                              NULL);
  /* TODO: GError */
  rest_proxy_call_async (call, flickr_callback, (GObject*)service, data, NULL);
}

static const char *
mojito_service_flickr_get_name (MojitoService *service)
{
  return "flickr";
}

static void
user_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  MojitoServiceFlickr *flickr = MOJITO_SERVICE_FLICKR (user_data);
  MojitoServiceFlickrPrivate *priv = flickr->priv;

  g_free (priv->user_id);
  priv->user_id = g_strdup (gconf_value_get_string (entry->value));
}

static void
mojito_service_flickr_dispose (GObject *object)
{
  MojitoServiceFlickrPrivate *priv = MOJITO_SERVICE_FLICKR (object)->priv;

  if (priv->gconf) {
    g_object_unref (priv->gconf);
    priv->gconf = NULL;
  }

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (mojito_service_flickr_parent_class)->dispose (object);
}

static void
mojito_service_flickr_finalize (GObject *object)
{
  MojitoServiceFlickrPrivate *priv = MOJITO_SERVICE_FLICKR (object)->priv;

  g_free (priv->user_id);

  G_OBJECT_CLASS (mojito_service_flickr_parent_class)->finalize (object);
}

static void
mojito_service_flickr_class_init (MojitoServiceFlickrClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoServiceClass *service_class = MOJITO_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoServiceFlickrPrivate));

  object_class->dispose = mojito_service_flickr_dispose;
  object_class->finalize = mojito_service_flickr_finalize;

  service_class->get_name = mojito_service_flickr_get_name;
  service_class->update = update;
}

static void
mojito_service_flickr_init (MojitoServiceFlickr *self)
{
  MojitoServiceFlickrPrivate *priv;

  self->priv = priv = GET_PRIVATE (self);

  priv->gconf = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf, KEY_BASE,
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_notify_add (priv->gconf, KEY_USER,
                           user_changed_cb, self, NULL, NULL);
  gconf_client_notify (priv->gconf, KEY_USER);

  priv->proxy = rest_proxy_new ("http://api.flickr.com/services/rest/", FALSE);
}
