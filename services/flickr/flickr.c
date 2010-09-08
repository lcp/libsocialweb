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

#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-online.h>
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
  gboolean inited; /* For GInitable */
  gboolean configured; /* Set if we have user tokens */
  gboolean authorised; /* Set if the tokens are valid */
};

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    CAN_VERIFY_CREDENTIALS,
    HAS_PHOTO_UPLOAD_IFACE,
    HAS_BANISHABLE_IFACE,
    HAS_QUERY_IFACE,

    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServiceFlickrPrivate *priv = GET_PRIVATE (service);

  static const char *unconfigured_caps[] = {
    NULL,
  };

  static const char *authorised_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_VALID,
    NULL
  };

  static const char *unauthorised_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_INVALID,
    NULL
  };

  if (priv->configured) {
    if (priv->authorised) {
      return authorised_caps;
    } else {
      return unauthorised_caps;
    }
  } else {
    return unconfigured_caps;
  }
}

static RestXmlNode *
node_from_call (RestProxyCall *call)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *node;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_warning (G_STRLOC ": error from Flickr: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  node = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));
  g_object_unref (call);

  /* Invalid XML, or incorrect root */
  if (node == NULL || !g_str_equal (node->name, "rsp")) {
    g_warning (G_STRLOC ": cannot make Flickr call");
    /* TODO: display the payload if its short */
    if (node) rest_xml_node_unref (node);
    return NULL;
  }

  if (g_strcmp0 (rest_xml_node_get_attr (node, "stat"), "ok") != 0) {
    RestXmlNode *err;
    err = rest_xml_node_find (node, "err");
    if (err)
      g_warning (G_STRLOC ": cannot make Flickr call: %s",
                 rest_xml_node_get_attr (err, "msg"));
    rest_xml_node_unref (node);
    return NULL;
  }

  return node;
}

static void
check_tokens_cb (RestProxyCall *call,
                 const GError  *error,
                 GObject       *weak_object,
                 gpointer       user_data)
{
  SwService *service = SW_SERVICE (weak_object);
  SwServiceFlickrPrivate *priv = GET_PRIVATE (service);
  RestXmlNode *root;

  root = node_from_call (call);
  if (root) {
    SW_DEBUG (FLICKR, "checkToken: authorised");
    priv->authorised = TRUE;
    rest_xml_node_unref (root);
  } else {
    SW_DEBUG (FLICKR, "checkToken: invalid token");
    priv->authorised = FALSE;
  }

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

static void
got_tokens_cb (RestProxy *proxy,
               gboolean   got_tokens,
               gpointer   user_data)
{
  SwService *service = SW_SERVICE (user_data);
  SwServiceFlickrPrivate *priv = GET_PRIVATE (service);
  RestProxyCall *call;

  SW_DEBUG (FLICKR, "Got tokens: %s", got_tokens ? "yes" : "no");

  priv->configured = got_tokens;
  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));

  if (got_tokens && sw_is_online ()) {
    call = rest_proxy_new_call (priv->proxy);
    rest_proxy_call_set_function (call, "flickr.auth.checkToken");
    rest_proxy_call_async (call, check_tokens_cb, G_OBJECT (service), NULL, NULL);
    /* TODO: error checking */
  }

  /* Drop reference we took for callback */
  g_object_unref (service);
}

static void
credentials_updated (SwService *service)
{
  SwServiceFlickrPrivate *priv = GET_PRIVATE (service);

  priv->configured = FALSE;
  priv->authorised = FALSE;

  sw_keyfob_flickr ((FlickrProxy *)priv->proxy,
                    got_tokens_cb,
                    g_object_ref (service)); /* ref gets dropped in cb */

  sw_service_emit_user_changed (service);
  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwService *service = SW_SERVICE (user_data);
  SwServiceFlickrPrivate *priv = GET_PRIVATE (service);

  SW_DEBUG (FLICKR, "Online: %s", online ? "yes" : "no");

  if (online) {
    got_tokens_cb (priv->proxy, TRUE, service);
  } else {
    priv->authorised = FALSE;

    sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
  }
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

  if (priv->inited)
    return TRUE;

  sw_keystore_get_key_secret ("flickr", &key, &secret);
  if (key == NULL || secret == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }

  priv->proxy = flickr_proxy_new (key, secret);

  sw_online_add_notify (online_notify, flickr);

  priv->inited = TRUE;

  credentials_updated (SW_SERVICE (flickr));

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

static void
on_upload_cb (RestProxyCall *call,
              const GError *error,
              GObject *weak_object,
              gpointer user_data)
{
  SwServiceFlickr *flickr = SW_SERVICE_FLICKR (weak_object);
  int opid = GPOINTER_TO_INT (user_data);

  if (error) {
    sw_photo_upload_iface_emit_photo_upload_progress (flickr, opid, -1, error->message);
    /* TODO: clean up */
  } else {
    /* TODO: check flickr error state */
    sw_photo_upload_iface_emit_photo_upload_progress (flickr, opid, 100, "");
  }
}

/* If @param_name exists in the DBus parameters, set @flickr_name on the
   RestProxyCall */
#define MAP_PARAM(param_name, flickr_name)                      \
  {                                                             \
    const char *param;                                          \
    param = g_hash_table_lookup (params_in, param_name);        \
    if (param)                                                  \
      rest_proxy_call_add_param (call, flickr_name, param);     \
  }

static void
_flickr_upload_photo (SwPhotoUploadIface    *self,
                      const gchar           *filename,
                      GHashTable            *params_in,
                      DBusGMethodInvocation *context)
{
  SwServiceFlickrPrivate *priv = GET_PRIVATE (self);
  GError *error = NULL;
  RestProxyCall *call;
  static int opid = 0;

  call = flickr_proxy_new_upload_for_file (FLICKR_PROXY (priv->proxy),
                                           filename,
                                           &error);

  if (error) {
    dbus_g_method_return_error (context, error);
    return;
  }

  /* Now add the parameters that we support */
  MAP_PARAM ("title", "title");
  MAP_PARAM ("x-flickr-is-public", "is_public");
  MAP_PARAM ("x-flickr-is-friend", "is_friend");
  MAP_PARAM ("x-flickr-is-family", "is_family");

  ++opid;

  rest_proxy_call_async (call, on_upload_cb, (GObject *)self, GINT_TO_POINTER (opid), NULL);

  sw_photo_upload_iface_return_from_upload_photo (context, opid);
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
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->get_static_caps = get_static_caps;
}

static void
sw_service_flickr_init (SwServiceFlickr *self)
{
  self->priv = GET_PRIVATE (self);
  self->priv->inited = FALSE;
  self->priv->configured = FALSE;
  self->priv->authorised = FALSE;
}
