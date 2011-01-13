/*
 * libsocialweb Plurk service support
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
#include <gnome-keyring.h>

#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-online.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <libsocialweb/sw-client-monitor.h>

#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include <interfaces/sw-query-ginterface.h>
#include <interfaces/sw-avatar-ginterface.h>
#include <interfaces/sw-status-update-ginterface.h>

#include "plurk.h"
#include "plurk-item-view.h"

static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void query_iface_init (gpointer g_iface, gpointer iface_data);
static void avatar_iface_init (gpointer g_iface, gpointer iface_data);
static void status_update_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServicePlurk,
                         sw_service_plurk,
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
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_PLURK, SwServicePlurkPrivate))

struct _SwServicePlurkPrivate {
  gboolean inited;
  enum {
    OFFLINE,
    CREDS_INVALID,
    CREDS_VALID
  } credentials;
  RestProxy *proxy;
  char *user_id;
  char *image_url;
  char *username, *password;
  char *api_key;
};

static void online_notify (gboolean online, gpointer user_data);
static void credentials_updated (SwService *service);

static JsonNode *
node_from_call (RestProxyCall *call, JsonParser *parser)
{
  JsonNode *root;
  GError *error;
  gboolean ret = FALSE;

  if (call == NULL)
    return NULL;

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_message ("Error from Plurk: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  ret = json_parser_load_from_data (parser,
                                    rest_proxy_call_get_payload (call),
                                    rest_proxy_call_get_payload_length (call),
                                    &error);
  root = json_parser_get_root (parser);

  if (root == NULL) {
    g_message ("Error from Plurk: %s",
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  return root;
}

static char *
construct_image_url (const char *uid,
                     const gint64 avatar,
                     const gint64 has_profile)
{
  char *url = NULL;

  if (has_profile == 1 && avatar <= 0)
    url = g_strdup_printf ("http://avatars.plurk.com/%s-medium.gif", uid);
  else if (has_profile == 1 && avatar > 0)
    url = g_strdup_printf ("http://avatars.plurk.com/%s-medium%lld.gif", uid, avatar);
  else
    url = g_strdup_printf ("http://www.plurk.com/static/default_medium.gif");

  return url;
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
  SwServicePlurkPrivate *priv = GET_PRIVATE (service);
  static const char *no_caps[] = { NULL };
  static const char *configured_caps[] = {
    IS_CONFIGURED,
    NULL
  };
  static const char *invalid_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_INVALID,
    NULL
  };
  static const char *full_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_VALID,
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };

  /* Check the conditions and determine which caps array to return */
  switch (priv->credentials) {
  case CREDS_VALID:
    return full_caps;
  case CREDS_INVALID:
    return invalid_caps;
  case OFFLINE:
    if (priv->username && priv->password)
      return configured_caps;
    else
      return no_caps;
  }

  /* Just in case we fell through that switch */
  g_warning ("Unhandled credential state %d", priv->credentials);
  return no_caps;
}

static void
construct_user_data (SwServicePlurk* plurk, JsonNode *root)
{
  SwServicePlurkPrivate *priv = GET_PRIVATE (plurk);
  JsonNode *node;
  JsonObject *object;
  const gchar *uid;
  gint64 id, avatar, has_profile;

  object = json_node_get_object (root);
  node = json_object_get_member (object, "user_info");

  if (!node)
    return;

  object = json_node_get_object (node);
  if (json_object_get_null_member (object, "uid"))
    return;

  id = json_object_get_int_member (object, "uid");

  avatar = json_object_get_int_member (object, "avatar");

  has_profile = json_object_get_int_member (object, "has_profile_image");

  uid = g_strdup_printf ("%lld", id);

  priv->user_id = (char *) uid;
  priv->image_url = construct_image_url (uid, avatar, has_profile);
}

static void
_got_login_data (RestProxyCall *call,
                 const GError  *error,
                 GObject       *weak_object,
                 gpointer       userdata)
{
  SwService *service = SW_SERVICE (weak_object);
  SwServicePlurk *plurk = SW_SERVICE_PLURK (service);
  JsonParser *parser = NULL;
  JsonNode *root;

  if (error) {
    // TODO sanity_check_date (call);
    g_message ("Error: %s", error->message);

    plurk->priv->credentials = CREDS_INVALID;
    sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));

    return;
  }

  plurk->priv->credentials = CREDS_VALID;

  parser = json_parser_new ();
  root = node_from_call (call, parser);
  construct_user_data (plurk, root);
  g_object_unref (root);

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));

  g_object_unref (call);
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwServicePlurk *plurk = (SwServicePlurk *)user_data;
  SwServicePlurkPrivate *priv = GET_PRIVATE (plurk);

  priv->credentials = OFFLINE;

  if (online) {
    if (priv->username && priv->password) {
      RestProxyCall *call;

      call = rest_proxy_new_call (priv->proxy);
      rest_proxy_call_set_function (call, "Users/login");
      rest_proxy_call_add_params (call,
                                  "api_key", priv->api_key,
                                  "username", priv->username,
                                  "password", priv->password,
                                  NULL);
      rest_proxy_call_async (call, _got_login_data, (GObject*)plurk, NULL, NULL);
    }
  } else {
    g_free (priv->user_id);
    priv->user_id = NULL;

    sw_service_emit_capabilities_changed ((SwService *)plurk,
                                          get_dynamic_caps ((SwService *)plurk));
  }
}

/*
 * Callback from the keyring lookup in refresh_credentials.
 */
static void
found_password_cb (GnomeKeyringResult  result,
                   GList              *list,
                   gpointer            user_data)
{
  SwService *service = SW_SERVICE (user_data);
  SwServicePlurk *plurk = SW_SERVICE_PLURK (service);
  SwServicePlurkPrivate *priv = plurk->priv;

  if (result == GNOME_KEYRING_RESULT_OK && list != NULL) {
    GnomeKeyringNetworkPasswordData *data = list->data;

    g_free (priv->username);
    g_free (priv->password);

    priv->username = g_strdup (data->user);
    priv->password = g_strdup (data->password);

    /* If we're online, force a reconnect to fetch new credentials */
    if (sw_is_online ()) {
      online_notify (FALSE, service);
      online_notify (TRUE, service);
    }
  } else {
    g_free (priv->username);
    g_free (priv->password);
    priv->username = NULL;
    priv->password = NULL;
    priv->credentials = OFFLINE;

    if (result != GNOME_KEYRING_RESULT_NO_MATCH) {
      g_warning (G_STRLOC ": Error getting password: %s", gnome_keyring_result_to_message (result));
    }
  }

  sw_service_emit_user_changed (service);
  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

static void
refresh_credentials (SwServicePlurk *plurk)
{
  gnome_keyring_find_network_password (NULL, NULL,
                                       "www.plurk.com",
                                       NULL, NULL, NULL, 0,
                                       found_password_cb, plurk, NULL);
}

static void
credentials_updated (SwService *service)
{
  refresh_credentials (SW_SERVICE_PLURK (service));
}

static const char *
sw_service_plurk_get_name (SwService *service)
{
  return "plurk";
}

static void
sw_service_plurk_dispose (GObject *object)
{
  SwServicePlurkPrivate *priv = SW_SERVICE_PLURK (object)->priv;

  sw_online_remove_notify (online_notify, object);

  /* unref private variables */
  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (sw_service_plurk_parent_class)->dispose (object);
}

static void
sw_service_plurk_finalize (GObject *object)
{
  /* Free private variables*/
  SwServicePlurkPrivate *priv = SW_SERVICE_PLURK (object)->priv;

  g_free (priv->user_id);
  g_free (priv->image_url);
  g_free (priv->username);
  g_free (priv->password);

  G_OBJECT_CLASS (sw_service_plurk_parent_class)->finalize (object);
}

static void
sw_service_plurk_class_init (SwServicePlurkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServicePlurkPrivate));

  object_class->dispose = sw_service_plurk_dispose;
  object_class->finalize = sw_service_plurk_finalize;

  service_class->get_name = sw_service_plurk_get_name;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_plurk_init (SwServicePlurk *self)
{
  self->priv = GET_PRIVATE (self);
  self->priv->inited = FALSE;
}

/* Initable interface */

static gboolean
sw_service_plurk_initable (GInitable    *initable,
                           GCancellable *cancellable,
                           GError      **error)
{
  /* Initialize the service and return TRUE if everything is OK.
     Otherwise, return FALSE */
  SwServicePlurk *plurk = SW_SERVICE_PLURK (initable);
  SwServicePlurkPrivate *priv = GET_PRIVATE (plurk);
  const char *key = NULL;

  if (priv->inited)
    return TRUE;

  sw_keystore_get_key_secret ("plurk", &key, NULL);
  if (key == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }

  priv->api_key = g_strdup (key);

  priv->credentials = OFFLINE;

  priv->proxy = rest_proxy_new ("http://www.plurk.com/API/", FALSE);

  sw_online_add_notify (online_notify, plurk);

  refresh_credentials (plurk);

  priv->inited = TRUE;

  return TRUE;

}

static void
initable_iface_init (gpointer g_iface, gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_plurk_initable;
}

/* Query interface */

static const gchar *valid_queries[] = {"feed",
                                       "own",
                                       "friends-only"};
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
_plurk_query_open_view (SwQueryIface          *self,
                        const gchar           *query,
                        GHashTable            *params,
                        DBusGMethodInvocation *context)
{
  SwServicePlurkPrivate *priv = GET_PRIVATE (self);
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

  item_view = g_object_new (SW_TYPE_PLURK_ITEM_VIEW,
                            "proxy", priv->proxy,
                            "api_key", priv->api_key,
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
                                      _plurk_query_open_view);
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
_plurk_avatar_request_avatar (SwAvatarIface         *self,
                              DBusGMethodInvocation *context)
{
  SwServicePlurkPrivate *priv = GET_PRIVATE (self);

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
                                            _plurk_avatar_request_avatar);
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
    sw_status_update_iface_emit_status_updated (weak_object, TRUE);
  }
}

static void
_plurk_status_update_update_status (SwStatusUpdateIface   *self,
                                    const gchar           *msg,
                                    GHashTable            *fields,
                                    DBusGMethodInvocation *context)
{
  SwServicePlurk *plurk = SW_SERVICE_PLURK (self);
  SwServicePlurkPrivate *priv = GET_PRIVATE (plurk);
  RestProxyCall *call;

  if (!priv->user_id)
    return;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_method (call, "POST");
  rest_proxy_call_set_function (call, "Timeline/plurkAdd");

  rest_proxy_call_add_params (call,
                              "api_key", priv->api_key,
                              "content", msg,
                              "qualifier", ":",
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
                                                  _plurk_status_update_update_status);
}

