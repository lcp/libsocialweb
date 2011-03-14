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
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gnome-keyring.h>

#include <libsocialweb/sw-service.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb/sw-call-list.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-client-monitor.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb-keystore/sw-keystore.h>

#include <rest/oauth-proxy.h>
#include <rest/rest-xml-parser.h>

#include <interfaces/sw-query-ginterface.h>

#include "vimeo.h"
#include "vimeo-item-view.h"

static void query_iface_init (gpointer g_iface, gpointer iface_data);
G_DEFINE_TYPE_WITH_CODE (SwServiceVimeo, sw_service_vimeo, SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (SW_TYPE_QUERY_IFACE, query_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_VIMEO, SwServiceVimeoPrivate))

struct _SwServiceVimeoPrivate {
  RestProxy *proxy;
  RestProxy *simple_proxy;

  gboolean configured;
  gboolean inited;

  gchar *username;
};

static const char *
get_name (SwService *service)
{
  return "vimeo";
}

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    HAS_QUERY_IFACE,
    HAS_BANISHABLE_IFACE,

    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServiceVimeoPrivate *priv = SW_SERVICE_VIMEO (service)->priv;

  static const char *configured_caps[] = {
    IS_CONFIGURED,
    NULL
  };

  static const char *authorized_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_VALID,
    NULL
  };

  static const char *no_caps[] = { NULL };

  if (priv->username != NULL)
    return authorized_caps;
  else if (priv->configured)
    return configured_caps;
  else
    return no_caps;
}

static RestXmlNode *
node_from_call (RestProxyCall *call, GError **error)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *node;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_set_error (error, SW_SERVICE_ERROR, SW_SERVICE_ERROR_REMOTE_ERROR,
                 "HTTP error: %s (%d)",
                 rest_proxy_call_get_status_message (call),
                 rest_proxy_call_get_status_code (call));
    return NULL;
  }

  node = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  /* Invalid XML, or incorrect root */
  if (node == NULL || !g_str_equal (node->name, "rsp")) {
    g_set_error (error, SW_SERVICE_ERROR, SW_SERVICE_ERROR_REMOTE_ERROR,
                 "malformed remote response: %s",
                 rest_proxy_call_get_payload (call));
    if (node)
      rest_xml_node_unref (node);
    return NULL;
  }

  if (g_strcmp0 (rest_xml_node_get_attr (node, "stat"), "ok") != 0) {
    RestXmlNode *err;
    err = rest_xml_node_find (node, "err");
    g_set_error (error, SW_SERVICE_ERROR, SW_SERVICE_ERROR_REMOTE_ERROR,
                 "remote Vimeo error: %s", err ?
                 rest_xml_node_get_attr (err, "msg") : "unknown");
    rest_xml_node_unref (node);
    return NULL;
  }

  return node;
}

static void
_check_access_token_cb (RestProxyCall *call,
                        const GError  *error,
                        GObject       *weak_object,
                        gpointer       user_data)
{
  RestXmlNode *root;
  GError *err = NULL;
  SwServiceVimeo *self = SW_SERVICE_VIMEO (weak_object);
  SwService *service = SW_SERVICE (self);
  SwServiceVimeoPrivate *priv = self->priv;

  root = node_from_call (call, &err);

  if (root == NULL) {
    g_assert (err != NULL);
    SW_DEBUG (VIMEO, "Invalid access token: %s", err->message);
    g_error_free (err);
  } else {
    RestXmlNode *username = rest_xml_node_find (root, "username");
    priv->username = g_strdup (username->content);
    rest_proxy_bind (priv->simple_proxy, priv->username);

    rest_xml_node_unref (root);
  }

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

static void
got_tokens_cb (RestProxy *proxy, gboolean got_tokens, gpointer user_data)
{
  SwServiceVimeo *self = (SwServiceVimeo *) user_data;
  SwService *service = SW_SERVICE (self);
  SwServiceVimeoPrivate *priv = self->priv;

  priv->configured = got_tokens;

  SW_DEBUG (VIMEO, "Got tokens: %s", got_tokens ? "yes" : "no");

  if (got_tokens) {
    RestProxyCall *call;

    call = rest_proxy_new_call (priv->proxy);
    rest_proxy_call_set_function(call, "api/rest/v2");

    rest_proxy_call_add_param (call, "method", "vimeo.test.login");

    rest_proxy_call_async (call,
                           _check_access_token_cb,
                           G_OBJECT (self),
                           NULL,
                           NULL);

    g_object_unref (call);
  }

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwServiceVimeo *self = (SwServiceVimeo *) user_data;
  SwService *service = SW_SERVICE (self);
  SwServiceVimeoPrivate *priv = self->priv;

  if (online) {
    sw_keyfob_oauth ((OAuthProxy *) priv->proxy, got_tokens_cb, self);
  } else {
    g_free (priv->username);
    priv->username = NULL;
    sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
  }
}

/*
 * The credentials have been updated (or we're starting up), so fetch them from
 * the keyring.
 */
static void
refresh_credentials (SwServiceVimeo *self)
{
  SwService *service = SW_SERVICE (self);
  SwServiceVimeoPrivate *priv = self->priv;

  SW_DEBUG (VIMEO, "Credentials updated");

  priv->configured = FALSE;

  sw_service_emit_user_changed (service);

  online_notify (FALSE, service);
  online_notify (TRUE, service);
}

/*
 * SwService:credentials_updated vfunc implementation
 */
static void
credentials_updated (SwService *service)
{
  refresh_credentials (SW_SERVICE_VIMEO (service));
}

static void
sw_service_vimeo_dispose (GObject *object)
{
  SwServiceVimeoPrivate *priv = ((SwServiceVimeo*)object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->simple_proxy) {
    g_object_unref (priv->simple_proxy);
    priv->simple_proxy = NULL;
  }

  g_free (priv->username);

  G_OBJECT_CLASS (sw_service_vimeo_parent_class)->dispose (object);
}


/* Query interface */

static const gchar *valid_queries[] = { "feed", "own" };

static gboolean
_check_query_validity (const gchar *query)
{
  gint i = 0;

  for (i = 0; i < G_N_ELEMENTS (valid_queries); i++) {
    if (g_str_equal (query, valid_queries[i]))
      return TRUE;
  }

  return FALSE;
}

static void
_vimeo_query_open_view (SwQueryIface          *self,
                        const gchar           *query,
                        GHashTable            *params,
                        DBusGMethodInvocation *context)
{
  SwServiceVimeoPrivate *priv = GET_PRIVATE (self);
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

  item_view = g_object_new (SW_TYPE_VIMEO_ITEM_VIEW,
                            "proxy", priv->simple_proxy,
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

  sw_query_iface_implement_open_view (klass, _vimeo_query_open_view);
}

static void
sw_service_vimeo_class_init (SwServiceVimeoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceVimeoPrivate));

  object_class->dispose = sw_service_vimeo_dispose;

  service_class->get_name = get_name;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_vimeo_init (SwServiceVimeo *self)
{
  SwServiceVimeoPrivate *priv;
  const gchar *api_key;
  const gchar *api_secret;

  priv = self->priv = GET_PRIVATE (self);

  sw_keystore_get_key_secret ("vimeo", &api_key, &api_secret);
  if (api_key == NULL || api_secret == NULL) {
    g_warning ("Could not get API key or secret");
  }

  priv->proxy = oauth_proxy_new (api_key, api_secret, "http://vimeo.com/", FALSE);
  priv->simple_proxy = rest_proxy_new ("http://vimeo.com/api/v2/%s/", TRUE);

  refresh_credentials (self);
}
