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
#include <glib/gi18n.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <rest-extras/flickr-proxy.h>
#include <rest/rest-xml-parser.h>
#include <interfaces/sw-query-ginterface.h>
#include <interfaces/sw-photo-upload-ginterface.h>

#include "flickr-item-view.h"
#include "flickr.h"


static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void query_iface_init (gpointer g_iface, gpointer iface_data);
static void photo_upload_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceFlickr, sw_service_flickr, SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_QUERY_IFACE, query_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_PHOTO_UPLOAD_IFACE, photo_upload_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_FLICKR, SwServiceFlickrPrivate))

struct _SwServiceFlickrPrivate {
  gboolean running;
  RestProxy *proxy;
  SoupSession *session; /* for upload */

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

  if (priv->session)
  {
    g_object_unref (priv->session);
    priv->session = NULL;
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

const static gchar *valid_queries[] = { "feed",
                                        "own",
                                        "friends-only",
                                        "x-search" };

static gboolean
_check_query_validity (const gchar *query)
{
  gint i = 0;

  for (i = 0; i < G_N_ELEMENTS (valid_queries); i++)
  {
    if (g_str_equal (query, valid_queries[i]))
      return TRUE;
  }

  return FALSE;
}

static void
_flickr_query_open_view (SwQueryIface          *self,
                         const gchar           *query,
                         GHashTable            *params,
                         DBusGMethodInvocation *context)
{
  SwServiceFlickrPrivate *priv = GET_PRIVATE (self);
  SwItemView *item_view;
  const gchar *object_path;

  if (!_check_query_validity (query))
  {
    dbus_g_method_return_error (context,
                                g_error_new (SW_SERVICE_ERROR,
                                             SW_SERVICE_ERROR_INVALID_QUERY,
                                             "Query '%s' is invalid",
                                             query));
    return;
  }

  item_view = g_object_new (SW_TYPE_FLICKR_ITEM_VIEW,
                            "proxy", priv->proxy,
                            "service", self,
                            "query", query,
                            "params", params,
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
                                      _flickr_query_open_view);
}

typedef struct
{
  SwServiceFlickr *service;
  gchar *filename;
  GHashTable *params;
  gint opid;
} UploadPhotoClosure;

static void
_upload_message_finished_cb (SoupMessage        *message,
                             UploadPhotoClosure *closure)
{
  RestXmlParser *parser;
  RestXmlNode *root;

  parser = rest_xml_parser_new ();
  root = rest_xml_parser_parse_from_data (parser,
                                          message->response_body->data,
                                          message->response_body->length);

  if (rest_xml_node_get_attr (root, "stat") && 
      g_str_equal (rest_xml_node_get_attr (root, "stat"), "ok"))
  {
    sw_photo_upload_iface_emit_photo_upload_progress (closure->service,
                                                      closure->opid,
                                                      100,
                                                      NULL);
  } else {
    sw_photo_upload_iface_emit_photo_upload_progress (closure->service,
                                                      closure->opid,
                                                      -1,
                                                      _("Upload failed"));
  }

  g_object_unref (parser);
  rest_xml_node_unref (root);

  g_free (closure->filename);
  g_object_unref (closure->service);
  g_hash_table_destroy (closure->params);
  g_free (closure);
}

static void
_tokens_fetched_for_upload_photo (RestProxy *proxy,
                                  gboolean   authorised,
                                  gpointer   user_data)
{
  UploadPhotoClosure *closure = (UploadPhotoClosure *)user_data;
  SwServiceFlickr *flickr = closure->service;
  SwServiceFlickrPrivate *priv = GET_PRIVATE (flickr);
  const gchar *error_msg;

  if (authorised)
  {
    const gchar *auth_token, *api_key;
    gchar *s;
    GHashTableIter iter;
    gpointer key, value;
    gchar *contents;
    gsize length;
    SoupBuffer *buf;
    GFile *file;
    GFileInfo *fi;
    const gchar *content_type;
    SoupMessage *message;
    SoupMultipart *mp;
    GError *error = NULL;

    /* Add extra keys to the params hash */
    auth_token = flickr_proxy_get_token ((FlickrProxy *)priv->proxy);
    api_key = flickr_proxy_get_api_key ((FlickrProxy *)priv->proxy);

    g_hash_table_insert (closure->params, "auth_token", (gchar *)auth_token);
    g_hash_table_insert (closure->params, "api_key", (gchar *)api_key);

    /* For testing */
    g_hash_table_insert (closure->params, "is_public", (gchar *)"0");

    /* Then sign it */
    s = flickr_proxy_sign ((FlickrProxy *)priv->proxy, closure->params);
    g_hash_table_insert (closure->params, "api_sig", s);

    /* Create a "multi-part" form from the params */
    mp = soup_multipart_new (SOUP_FORM_MIME_TYPE_MULTIPART);

    g_hash_table_iter_init (&iter, closure->params);

    while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const gchar *control_name = (const gchar *)key;
      const gchar *data = (const gchar *)value;

      soup_multipart_append_form_string (mp, control_name, data);
    }

    g_free (s);

    /* Load contents into buffer. We don't free 'contents' since we use
     * SOUP_MEMORY_TAKE when creating the buffer */
    if (!g_file_get_contents (closure->filename, &contents, &length, &error))
    {
      g_warning (G_STRLOC ": Error getting file contents: %s", error->message);
      g_clear_error (&error);
      error_msg = _("Error opening file");
      goto fail;
    }

    buf = soup_buffer_new (SOUP_MEMORY_TAKE, contents, length);

    /* Find content type */
    file = g_file_new_for_path (closure->filename);
    fi = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            &error);

    if (!fi)
    {
      g_warning (G_STRLOC ": Error getting file info: %s", error->message);
      g_clear_error (&error);
      error_msg = _("Error getting file type");
      goto fail;
    }
  
    content_type = g_file_info_get_attribute_string (fi, G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);

    /* Now for the body, we need to use control name "photo"
     * http://www.flickr.com/services/api/upload.example.html
     */
    soup_multipart_append_form_file (mp,
                                     "photo",
                                     closure->filename,
                                     content_type,
                                     buf);

    message = soup_form_request_new_from_multipart ("http://api.flickr.com/services/upload/",
                                                    mp);

    g_signal_connect (message,
                      "finished",
                      (GCallback)_upload_message_finished_cb,
                      closure);
    soup_session_queue_message (priv->session, message, NULL, NULL);
  } else {
    error_msg = _("Unauthorised");
    goto fail;
  }

  return;

fail:
  sw_photo_upload_iface_emit_photo_upload_progress (closure->service,
                                                    closure->opid,
                                                    -1,
                                                    error_msg);
  g_free (closure->filename);
  g_object_unref (closure->service);
  g_hash_table_destroy (closure->params);
  g_free (closure);
}


static void
_flickr_upload_photo (SwPhotoUploadIface    *self,
                      const gchar           *filename,
                      GHashTable            *params_in,
                      DBusGMethodInvocation *context)
{
  SwServiceFlickrPrivate *priv = GET_PRIVATE (self);
  UploadPhotoClosure *closure;
  static gint opid = 0;

  closure = g_new0 (UploadPhotoClosure, 1);

  closure->filename = g_strdup (filename);
  closure->params = g_hash_table_new (g_str_hash, g_str_equal);
  closure->service = g_object_ref (self);
  closure->opid = ++opid;

  /* TODO: Copy supported params from params_in */

  sw_keyfob_flickr ((FlickrProxy*)priv->proxy, 
                    _tokens_fetched_for_upload_photo,
                    closure);

  sw_photo_upload_iface_return_from_upload_photo (context, closure->opid);
}


static void
photo_upload_iface_init (gpointer g_iface,
                         gpointer iface_data)
{
  SwPhotoUploadIfaceClass *klass = (SwPhotoUploadIfaceClass *)g_iface;

  sw_photo_upload_iface_implement_upload_photo (klass,
                                                _flickr_upload_photo);
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
  self->priv->session = soup_session_async_new ();
  service_list = g_list_prepend (service_list, self);
}
