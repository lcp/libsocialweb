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

#include <rest/rest-proxy.h>
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

/*
 * Callback from the keyring lookup in refresh_credentials.
 */
static void
found_password_cb (GnomeKeyringResult  result,
                   GList              *list,
                   gpointer            user_data)
{
  SwService *service = SW_SERVICE (user_data);
  SwServiceVimeo *vimeo = SW_SERVICE_VIMEO (service);
  SwServiceVimeoPrivate *priv = vimeo->priv;

  if (result == GNOME_KEYRING_RESULT_OK && list != NULL) {
    GnomeKeyringNetworkPasswordData *data = list->data;
    rest_proxy_bind (priv->proxy, data->user);
  } else {
    rest_proxy_bind (priv->proxy, "");

    if (result != GNOME_KEYRING_RESULT_NO_MATCH) {
      g_warning (G_STRLOC ": Error getting password: %s", gnome_keyring_result_to_message (result));
    }
  }

  sw_service_emit_user_changed (service);
  /* TODO: dynamic caps */
}

/*
 * The credentials have been updated (or we're starting up), so fetch them from
 * the keyring.
 */
static void
refresh_credentials (SwServiceVimeo *vimeo)
{
  SW_DEBUG (VIMEO, "Credentials updated");

  gnome_keyring_find_network_password (NULL, NULL,
                                       "vimeo.com",
                                       NULL, NULL, NULL, 0,
                                       found_password_cb, vimeo, NULL);
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
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_vimeo_init (SwServiceVimeo *self)
{
  SwServiceVimeoPrivate *priv;

  priv = self->priv = GET_PRIVATE (self);

  priv->proxy = rest_proxy_new ("http://vimeo.com/api/v2/%s/", TRUE);

  refresh_credentials (self);
}
