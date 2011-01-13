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
#include <glib/gi18n.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-online.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-client-monitor.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb-keystore/sw-keystore.h>

#include <rest/oauth-proxy.h>
#include <rest/oauth-proxy-call.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>

#include <interfaces/sw-query-ginterface.h>
#include <interfaces/sw-avatar-ginterface.h>
#include <interfaces/sw-status-update-ginterface.h>

#include "sina.h"
#include "sina-item-view.h"

static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void query_iface_init (gpointer g_iface, gpointer iface_data);
static void avatar_iface_init (gpointer g_iface, gpointer iface_data);
static void status_update_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceSina,
                         sw_service_sina,
                         SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_QUERY_IFACE,
                                                query_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_AVATAR_IFACE,
                                                avatar_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_STATUS_UPDATE_IFACE,
                                                status_update_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_SINA, SwServiceSinaPrivate))

struct _SwServiceSinaPrivate {
  gboolean inited;
  RestProxy *proxy;
  char *user_id;
  char *image_url;
};

static void online_notify (gboolean online, gpointer user_data);
static void credentials_updated (SwService *service);

static gboolean
account_is_configured ()
{
  RestProxy *proxy;
  gboolean configured = FALSE;
  const char *key = NULL, *secret = NULL;

  sw_keystore_get_key_secret ("sina", &key, &secret);
  proxy = oauth_proxy_new (key, secret, "http://api.t.sina.com.cn/", FALSE);

  configured = sw_keyfob_oauth_sync ((OAuthProxy *)proxy);

  g_object_unref (proxy);

  return configured;
}

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    CAN_VERIFY_CREDENTIALS,
    HAS_UPDATE_STATUS_IFACE,
    HAS_AVATAR_IFACE,
    HAS_BANISHABLE_IFACE,
    HAS_QUERY_IFACE,

    /* deprecated */
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServiceSinaPrivate *priv = SW_SERVICE_SINA (service)->priv;
  gboolean configured = FALSE;
  static const char *no_caps[] = { NULL };
  static const char *configured_caps[] = {
    IS_CONFIGURED,
    NULL
  };
  static const char *full_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_VALID,
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };

  if (priv->user_id)
    return full_caps;

  configured = account_is_configured ();

  if (configured)
    return configured_caps;
  else
    return no_caps;
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

static void got_tokens_cb (RestProxy *proxy, gboolean authorised, gpointer user_data);

static void
got_user_cb (RestProxyCall *call,
             const GError  *error,
             GObject       *weak_object,
             gpointer       userdata)
{
  SwService *service = SW_SERVICE (weak_object);
  SwServiceSina *sina = SW_SERVICE_SINA (service);
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);

  RestXmlNode *root;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  root = xml_node_from_call (call, "Sina");
  if (!root)
    return;

  priv->user_id = xml_get_child_node_value (root, "id");
  priv->image_url = xml_get_child_node_value (root, "profile_image_url");

  rest_xml_node_unref (root);

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

static void
got_tokens_cb (RestProxy *proxy, gboolean authorised, gpointer user_data)
{
  SwServiceSina *sina = SW_SERVICE_SINA (user_data);
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);
  RestProxyCall *call;

  if (authorised) {
    call = rest_proxy_new_call (priv->proxy);
    rest_proxy_call_set_function (call, "account/verify_credentials.xml");
    rest_proxy_call_async (call, got_user_cb, (GObject*)sina, NULL, NULL);
  }
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwServiceSina *sina = SW_SERVICE_SINA (user_data);
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);

  if (online) {
    sw_keyfob_oauth ((OAuthProxy *)priv->proxy, got_tokens_cb, sina);
  } else {
    g_free (priv->user_id);
    priv->user_id = NULL;

    g_free (priv->image_url);
    priv->image_url = NULL;

    sw_service_emit_capabilities_changed ((SwService *)sina,
                                          get_dynamic_caps ((SwService *)sina));
  }
}

static void
refresh_credentials (SwServiceSina *sina)
{
  online_notify (FALSE, (SwService *)sina);
  online_notify (TRUE, (SwService *)sina);
}

static void
credentials_updated (SwService *service)
{
  refresh_credentials (SW_SERVICE_SINA (service));

  sw_service_emit_user_changed (service);
  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

static const char *
sw_service_sina_get_name (SwService *service)
{
  return "sina";
}

static void
sw_service_sina_dispose (GObject *object)
{
  SwServiceSinaPrivate *priv = SW_SERVICE_SINA (object)->priv;

  sw_online_remove_notify (online_notify, object);

  /* unref private variables */
  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (sw_service_sina_parent_class)->dispose (object);
}

static void
sw_service_sina_finalize (GObject *object)
{
  SwServiceSinaPrivate *priv = SW_SERVICE_SINA (object)->priv;

  g_free (priv->user_id);
  g_free (priv->image_url);

  G_OBJECT_CLASS (sw_service_sina_parent_class)->finalize (object);
}

static void
sw_service_sina_class_init (SwServiceSinaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceSinaPrivate));

  object_class->dispose = sw_service_sina_dispose;
  object_class->finalize = sw_service_sina_finalize;

  service_class->get_name = sw_service_sina_get_name;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_sina_init (SwServiceSina *self)
{
  self->priv = GET_PRIVATE (self);
  self->priv->inited = FALSE;
}

/* Initable interface */

static gboolean
sw_service_sina_initable (GInitable    *initable,
                          GCancellable *cancellable,
                          GError      **error)
{
  /* Initialize the service and return TRUE if everything is OK.
     Otherwise, return FALSE */
  SwServiceSina *sina = SW_SERVICE_SINA (initable);
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);
  const char *key = NULL, *secret = NULL;

  if (priv->inited)
    return TRUE;

  sw_keystore_get_key_secret ("sina", &key, &secret);
  if (key == NULL || secret == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }
  priv->proxy = oauth_proxy_new (key, secret, "http://api.t.sina.com.cn/", FALSE);

  sw_online_add_notify (online_notify, sina);

  refresh_credentials (sina);

  priv->inited = TRUE;

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface, gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_sina_initable;
}

/* Query interface */

static const gchar *valid_queries[] = {"feed",
                                       "own"};

static gboolean
_check_query_validity (const gchar *query)
{
  gint i = 0;

  for (i = 0; i < G_N_ELEMENTS(valid_queries); i++)
  {
    if (g_str_equal (query, valid_queries[i]))
      return TRUE;
  }

  return FALSE;
}

static void
_sina_query_open_view (SwQueryIface          *self,
                       const gchar           *query,
                       GHashTable            *params,
                       DBusGMethodInvocation *context)
{
  SwServiceSinaPrivate *priv = GET_PRIVATE (self);
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

  item_view = g_object_new (SW_TYPE_SINA_ITEM_VIEW,
                            "proxy", priv->proxy,
                            "service", self,
                            "query", query,
                            "params", params,
                            NULL);

  object_path = sw_item_view_get_object_path (item_view);
  /* Ensure the object gets disposed when the client goes away */
  sw_client_monitor_add (dbus_g_method_get_sender (context),
                         (GObject *)item_view);
  sw_query_iface_return_from_open_view (context,
                                        object_path);
}

static void
query_iface_init (gpointer g_iface,
                  gpointer iface_data)
{
  SwQueryIfaceClass *klass = (SwQueryIfaceClass*)g_iface;

  sw_query_iface_implement_open_view (klass,
                                      _sina_query_open_view);
}

/* Avatar interface */
static void
_requested_avatar_downloaded_cb (const gchar *uri,
                                 gchar       *local_path,
                                 gpointer     userdata)
{
  SwService *service = SW_SERVICE (userdata);

  sw_avatar_iface_emit_avatar_retrieved (service, local_path);
  g_free (local_path);
}

static void
_sina_avatar_request_avatar (SwAvatarIface         *self,
                             DBusGMethodInvocation *context)
{
  SwServiceSinaPrivate *priv = GET_PRIVATE (self);

  if (priv->image_url) {
    sw_web_download_image_async (priv->image_url,
                                 _requested_avatar_downloaded_cb,
                                 self);
  }

  sw_avatar_iface_return_from_request_avatar (context);
}

static void
avatar_iface_init (gpointer g_iface,
                   gpointer iface_data)
{
  SwAvatarIfaceClass *klass = (SwAvatarIfaceClass*)g_iface;

  sw_avatar_iface_implement_request_avatar (klass,
                                            _sina_avatar_request_avatar);
}

/* Status Update interface */
static void
_update_status_cb (RestProxyCall *call,
                   const GError  *error,
                   GObject       *weak_object,
                   gpointer       userdata)
{
  if (error)
  {
    g_critical (G_STRLOC ": Error updating status: %s",
                error->message);
    sw_status_update_iface_emit_status_updated (weak_object, FALSE);
  } else {
    SW_DEBUG (TWITTER, G_STRLOC ": Status updated.");
    sw_status_update_iface_emit_status_updated (weak_object, TRUE);
  }
}

static void
_sina_status_update_update_status (SwStatusUpdateIface   *self,
                                   const gchar           *msg,
                                   GHashTable            *fields,
                                   DBusGMethodInvocation *context)
{
  SwServiceSina *sina = SW_SERVICE_SINA (self);
  SwServiceSinaPrivate *priv = GET_PRIVATE (sina);
  RestProxyCall *call;

  if (!priv->user_id)
    return;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_method (call, "POST");
  rest_proxy_call_set_function (call, "statuses/update.xml");

  rest_proxy_call_add_params (call,
                              "status", msg,
                              NULL);

  rest_proxy_call_async (call, _update_status_cb, (GObject *)self, NULL, NULL);
  sw_status_update_iface_return_from_update_status (context);
}

static void
status_update_iface_init (gpointer g_iface,
                          gpointer iface_data)
{
  SwStatusUpdateIfaceClass *klass = (SwStatusUpdateIfaceClass*)g_iface;

  sw_status_update_iface_implement_update_status (klass,
                                                  _sina_status_update_update_status);
}

