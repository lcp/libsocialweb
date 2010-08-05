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
#include <dbus/dbus-glib-lowlevel.h>

#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb/sw-client-monitor.h>

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
  RestProxy *proxy;
  SoupSession *session; /* for upload */
};

static void
got_tokens_cb (RestProxy *proxy,
               gboolean   authorised,
               gpointer   user_data)
{
  SwServiceFlickr *flickr = SW_SERVICE_FLICKR (user_data);

  if (authorised) {
    /* TODO: this assumes that the tokens are valid. we should call checkToken
       and re-auth if required. */
  }

  /* Drop reference we took for callback */
  g_object_unref (flickr);
}

static void
credentials_updated (SwService *service)
{
  SwServiceFlickr *flickr = (SwServiceFlickr *)service;

  sw_keyfob_flickr ((FlickrProxy *)flickr->priv->proxy, 
                    got_tokens_cb,
                    g_object_ref (service)); /* ref gets dropped in cb */
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
                                        "x-flickr-search" };

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

  /* Ensure the object gets disposed when the client goes away */
  sw_client_monitor_add (dbus_g_method_get_sender (context),
                         (GObject *)item_view);

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
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_flickr_init (SwServiceFlickr *self)
{
  self->priv = GET_PRIVATE (self);
  self->priv->session = soup_session_async_new ();
}
