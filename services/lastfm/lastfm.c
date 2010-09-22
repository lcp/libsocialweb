/*
 * libsocialweb - social data store
 * Copyright (C) 2009 Intel Corporation.
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
#include <gnome-keyring.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <libsocialweb/sw-service.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb/sw-call-list.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <libsocialweb/sw-client-monitor.h>

#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>

#include <interfaces/sw-query-ginterface.h>
#include <interfaces/lastfm-ginterface.h>

#include "lastfm.h"
#include "lastfm-item-view.h"

static void lastfm_iface_init (gpointer g_iface, gpointer iface_data);
static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void query_iface_init (gpointer g_iface, gpointer iface_data);
G_DEFINE_TYPE_WITH_CODE (SwServiceLastfm, sw_service_lastfm, SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_LASTFM_IFACE, lastfm_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_QUERY_IFACE, query_iface_init));


#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_LASTFM, SwServiceLastfmPrivate))

struct _SwServiceLastfmPrivate {
  RestProxy *proxy;
  char *username;
};

static const char ** get_dynamic_caps (SwService *service);

static void
lastfm_now_playing (SwLastfmIface *self,
                    const gchar *in_artist,
                    const gchar *in_album,
                    const gchar *in_track,
                    guint in_length,
                    guint in_tracknumber,
                    const gchar *in_musicbrainz,
                    DBusGMethodInvocation *context)
{
  /* TODO */
  sw_lastfm_iface_return_from_now_playing (context);
}

static void
lastfm_submit_track (SwLastfmIface *self,
                     const gchar *in_artist,
                     const gchar *in_album,
                     const gchar *in_track,
                     gint64 in_time,
                     const gchar *in_source,
                     const gchar *in_rating,
                     guint in_length,
                     guint in_tracknumber,
                     const gchar *in_musicbrainz,
                     DBusGMethodInvocation *context)
{
  /* TODO */
  sw_lastfm_iface_return_from_submit_track (context);
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
  SwServiceLastfm *lastfm = SW_SERVICE_LASTFM (service);
  SwServiceLastfmPrivate *priv = lastfm->priv;

  if (result == GNOME_KEYRING_RESULT_OK && list != NULL) {
    GnomeKeyringNetworkPasswordData *data = list->data;

    g_free (priv->username);
    priv->username = g_strdup (data->user);
  } else {
    g_free (priv->username);
    priv->username = NULL;

    if (result != GNOME_KEYRING_RESULT_NO_MATCH) {
      g_warning (G_STRLOC ": Error getting password: %s", gnome_keyring_result_to_message (result));
    }
  }

  sw_service_emit_user_changed (service);
  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

/*
 * The credentials have been updated (or we're starting up), so fetch them from
 * the keyring.
 */
static void
refresh_credentials (SwServiceLastfm *lastfm)
{
  gnome_keyring_find_network_password (NULL, NULL,
                                       "ws.audioscrobbler.com",
                                       NULL, NULL, NULL, 0,
                                       found_password_cb, lastfm, NULL);
}

/*
 * SwService:credentials_updated vfunc implementation
 */
static void
credentials_updated (SwService *service)
{
  SW_DEBUG (LASTFM, "Credentials updated");

  refresh_credentials (SW_SERVICE_LASTFM (service));
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServiceLastfmPrivate *priv = GET_PRIVATE (service);

  static const char *configured_caps[] = {
    IS_CONFIGURED,
    CREDENTIALS_VALID,
    NULL
  };

  static const char *unconfigured_caps[] = {
    NULL
  };

  if (priv->username)
  {
    return configured_caps;
  } else {
    return unconfigured_caps;
  }
}

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    CAN_VERIFY_CREDENTIALS,
    HAS_QUERY_IFACE,
    HAS_BANISHABLE_IFACE,

    NULL
  };

  return caps;
}

static const char *
get_name (SwService *service)
{
  return "lastfm";
}

static void
sw_service_lastfm_dispose (GObject *object)
{
  SwServiceLastfmPrivate *priv = ((SwServiceLastfm*)object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (sw_service_lastfm_parent_class)->dispose (object);
}

static void
sw_service_lastfm_finalize (GObject *object)
{
  SwServiceLastfmPrivate *priv = ((SwServiceLastfm*)object)->priv;

  g_free (priv->username);

  G_OBJECT_CLASS (sw_service_lastfm_parent_class)->finalize (object);
}

static gboolean
sw_service_lastfm_initable (GInitable     *initable,
                            GCancellable  *cancellable,
                            GError       **error)
{
  SwServiceLastfm *lastfm = SW_SERVICE_LASTFM (initable);
  SwServiceLastfmPrivate *priv = lastfm->priv;

  if (sw_keystore_get_key ("lastfm") == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }

  if (priv->proxy)
    return TRUE;

  priv->proxy = rest_proxy_new ("http://ws.audioscrobbler.com/2.0/", FALSE);

  refresh_credentials (lastfm);

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface,
                     gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_lastfm_initable;
}

static void
lastfm_iface_init (gpointer g_iface,
                   gpointer iface_data)
{
  SwLastfmIfaceClass *klass = (SwLastfmIfaceClass *)g_iface;

  sw_lastfm_iface_implement_submit_track (klass, lastfm_submit_track);
  sw_lastfm_iface_implement_now_playing (klass, lastfm_now_playing);
}

/* Query interface */

static const gchar *valid_queries[] = { "feed" };

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
_lastfm_query_open_view (SwQueryIface          *self,
                         const gchar           *query,
                         GHashTable            *params,
                         DBusGMethodInvocation *context)
{
  SwServiceLastfmPrivate *priv = GET_PRIVATE (self);
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

  item_view = g_object_new (SW_TYPE_LASTFM_ITEM_VIEW,
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
                                      _lastfm_query_open_view);
}

static void
sw_service_lastfm_class_init (SwServiceLastfmClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceLastfmPrivate));

  object_class->dispose = sw_service_lastfm_dispose;
  object_class->finalize = sw_service_lastfm_finalize;

  service_class->get_name = get_name;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->get_static_caps = get_static_caps;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_lastfm_init (SwServiceLastfm *self)
{
  self->priv = GET_PRIVATE (self);
}


const gchar *
sw_service_lastfm_get_user_id (SwServiceLastfm *service)
{
  return service->priv->username;
}
