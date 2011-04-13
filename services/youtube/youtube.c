/*
 * libsocialweb Youtube service support
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

#include <rest-extras/youtube-proxy.h>
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>

#include <interfaces/sw-query-ginterface.h>
#include <interfaces/sw-video-upload-ginterface.h>

#include "youtube.h"
#include "youtube-item-view.h"

static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void query_iface_init (gpointer g_iface, gpointer iface_data);
static void video_upload_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceYoutube,
                         sw_service_youtube,
                         SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_QUERY_IFACE,
                                                query_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_VIDEO_UPLOAD_IFACE,
                                                video_upload_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_YOUTUBE, SwServiceYoutubePrivate))

static const ParameterNameMapping upload_params[] = {
  { "title", "title" },
  { "description", "description" },
  { "x-youtube-category", "category" },
  { "x-youtube-keywords", "keywords" },
  { NULL, NULL }
};

struct _SwServiceYoutubePrivate {
  gboolean inited;
  enum {
    OFFLINE,
    CREDS_INVALID,
    CREDS_VALID
  } credentials;
  RestProxy *proxy;
  RestProxy *auth_proxy;
  RestProxy *upload_proxy;
  char *username;
  char *password;
  char *developer_key;
  char *user_auth;
  char *nickname;
};

static void online_notify (gboolean online, gpointer user_data);
static void credentials_updated (SwService *service);

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    CAN_VERIFY_CREDENTIALS,
    HAS_QUERY_IFACE,
    HAS_BANISHABLE_IFACE,
    HAS_VIDEO_UPLOAD_IFACE,

    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServiceYoutubePrivate *priv = GET_PRIVATE (service);
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
    NULL
  };

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

const char *
sw_service_youtube_get_user_auth (SwServiceYoutube *youtube)
{
  SwServiceYoutubePrivate *priv = GET_PRIVATE (youtube);

  return priv->user_auth;
}

static void
_got_user_auth (RestProxyCall *call,
                const GError  *error,
                GObject       *weak_object,
                gpointer       user_data)
{
  SwService *service = SW_SERVICE (weak_object);
  SwServiceYoutubePrivate *priv = SW_SERVICE_YOUTUBE (service)->priv;
  const char *message = rest_proxy_call_get_payload (call);
  char **tokens;

  if (error) {
    g_message ("Error: %s", error->message);
    g_message ("Error from Youtube: %s", message);
    priv->credentials = CREDS_INVALID;
    sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
    return;
  }

  /* Parse the message */
  tokens = g_strsplit_set (message, "=\n", -1);
  if (g_strcmp0 (tokens[0], "Auth") == 0 &&
      g_strcmp0 (tokens[2], "YouTubeUser") == 0) {
    priv->user_auth = g_strdup (tokens[1]);
    /*Alright! we got the auth!!!*/
    youtube_proxy_set_user_auth (YOUTUBE_PROXY (priv->upload_proxy),
                                 priv->user_auth);
    priv->nickname = g_strdup (tokens[3]);
    priv->credentials = CREDS_VALID;
  } else {
    priv->credentials = CREDS_INVALID;
  }

  g_strfreev(tokens);

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));

  g_object_unref (call);
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwServiceYoutube *youtube = (SwServiceYoutube *)user_data;
  SwServiceYoutubePrivate *priv = GET_PRIVATE (youtube);

  priv->credentials = OFFLINE;

  if (online) {
    if (priv->username && priv->password) {
      RestProxyCall *call;

      /* request user_auth */
      /* http://code.google.com/intl/zh-TW/apis/youtube/2.0/developers_guide_protocol_clientlogin.html */
      call = rest_proxy_new_call (priv->auth_proxy);
      rest_proxy_call_set_method (call, "POST");
      rest_proxy_call_set_function (call, "ClientLogin");
      rest_proxy_call_add_params (call,
                                  "Email", priv->username,
                                  "Passwd", priv->password,
                                  "service", "youtube",
                                  "source", "SUSE MeeGo",
                                  NULL);
      rest_proxy_call_add_header (call,
                                  "Content-Type",
                                  "application/x-www-form-urlencoded");
      rest_proxy_call_async (call,
                             (RestProxyCallAsyncCallback)_got_user_auth,
                             (GObject*)youtube,
                             NULL,
                             NULL);
    }
  } else {
    sw_service_emit_capabilities_changed ((SwService *)youtube,
                                          get_dynamic_caps ((SwService *)youtube));
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
  SwServiceYoutube *youtube = SW_SERVICE_YOUTUBE (service);
  SwServiceYoutubePrivate *priv = GET_PRIVATE (youtube);

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

/*
 * The credentials have been updated (or we're starting up), so fetch them from
 * the keyring.
 */
static void
refresh_credentials (SwServiceYoutube *youtube)
{
  gnome_keyring_find_network_password (NULL, NULL,
                                       "www.youtube.com",
                                       NULL, NULL, NULL, 0,
                                       found_password_cb, youtube, NULL);
}

static void
credentials_updated (SwService *service)
{
  refresh_credentials (SW_SERVICE_YOUTUBE (service));
}

static const char *
sw_service_youtube_get_name (SwService *service)
{
  return "youtube";
}

static void
sw_service_youtube_dispose (GObject *object)
{
  SwServiceYoutubePrivate *priv = SW_SERVICE_YOUTUBE (object)->priv;

  sw_online_remove_notify (online_notify, object);

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->auth_proxy) {
    g_object_unref (priv->auth_proxy);
    priv->auth_proxy = NULL;
  }

  if (priv->upload_proxy) {
    g_object_unref (priv->upload_proxy);
    priv->upload_proxy = NULL;
  }

  G_OBJECT_CLASS (sw_service_youtube_parent_class)->dispose (object);
}

static void
sw_service_youtube_finalize (GObject *object)
{
  SwServiceYoutubePrivate *priv = SW_SERVICE_YOUTUBE (object)->priv;

  g_free (priv->username);
  g_free (priv->password);
  g_free (priv->user_auth);
  g_free (priv->developer_key);
  g_free (priv->nickname);

  G_OBJECT_CLASS (sw_service_youtube_parent_class)->finalize (object);
}

static void
sw_service_youtube_class_init (SwServiceYoutubeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceYoutubePrivate));

  object_class->dispose = sw_service_youtube_dispose;
  object_class->finalize = sw_service_youtube_finalize;

  service_class->get_name = sw_service_youtube_get_name;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_youtube_init (SwServiceYoutube *self)
{
  self->priv = GET_PRIVATE (self);

  self->priv->inited = FALSE;
  self->priv->username = NULL;
  self->priv->password = NULL;
  self->priv->user_auth = NULL;
  self->priv->nickname = NULL;
}

/* Initable interface */

static gboolean
sw_service_youtube_initable (GInitable    *initable,
                             GCancellable *cancellable,
                             GError      **error)
{
  SwServiceYoutube *youtube = SW_SERVICE_YOUTUBE (initable);
  SwServiceYoutubePrivate *priv = GET_PRIVATE (youtube);
  const char *key = NULL;

  if (priv->inited)
    return TRUE;

  sw_keystore_get_key_secret ("youtube", &key, NULL);
  if (key == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }

  priv->proxy = rest_proxy_new ("http://gdata.youtube.com/feeds/api/", FALSE);
  priv->auth_proxy = rest_proxy_new ("https://www.google.com/youtube/accounts/", FALSE);
  priv->upload_proxy = youtube_proxy_new (key);

  priv->developer_key = (char *)key;
  priv->credentials = OFFLINE;

  sw_online_add_notify (online_notify, youtube);

  refresh_credentials (youtube);

  priv->inited = TRUE;

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface, gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_youtube_initable;
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
_youtube_query_open_view (SwQueryIface          *self,
                          const gchar           *query,
                          GHashTable            *params,
                          DBusGMethodInvocation *context)
{
  SwServiceYoutubePrivate *priv = GET_PRIVATE (self);
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

  item_view = g_object_new (SW_TYPE_YOUTUBE_ITEM_VIEW,
                            "proxy", priv->proxy,
                            "developer_key", priv->developer_key,
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

  sw_query_iface_implement_open_view (klass, _youtube_query_open_view);
}

/* Video upload iface */

static void
_video_upload_cb (YoutubeProxy   *proxy,
                  const gchar   *payload,
                  gsize          total,
                  gsize          uploaded,
                  const GError  *error,
                  GObject       *weak_object,
                  gpointer       user_data)
{
  SwServiceYoutube *self = SW_SERVICE_YOUTUBE (weak_object);
  int opid = GPOINTER_TO_INT (user_data);

  if (error) {
    sw_video_upload_iface_emit_video_upload_progress (self, opid, -1,
        error->message);
  } else {
    gint percent = (gdouble) uploaded / (gdouble) total * 100;
    sw_video_upload_iface_emit_video_upload_progress (self, opid, percent, "");
  }
}

static void
_youtube_upload_video (SwVideoUploadIface    *iface,
                       const gchar           *filename,
                       GHashTable            *fields,
                       DBusGMethodInvocation *context)
{
  SwServiceYoutube *self = SW_SERVICE_YOUTUBE (iface);
  SwServiceYoutubePrivate *priv = self->priv;
  GError *error = NULL;
  GHashTable *native_fields = g_hash_table_new (g_str_hash, g_str_equal);
  gint opid = sw_next_opid ();

  sw_video_upload_iface_return_from_upload_video (context, opid);

  sw_service_map_params (upload_params, fields,
                         (SwServiceSetParamFunc) g_hash_table_insert,
                         native_fields);

  /* HACK: Seems like <yt:incomplete/> does not work well with categories */
  if (g_hash_table_lookup (native_fields, "category") == NULL)
    g_hash_table_insert (native_fields, "category", "People");

  if (!youtube_proxy_upload_async (YOUTUBE_PROXY (priv->upload_proxy),
                                   filename, native_fields, TRUE,
                                   _video_upload_cb, G_OBJECT (self),
                                   GINT_TO_POINTER (opid), &error)) {
    sw_video_upload_iface_emit_video_upload_progress (self, opid, -1,
                                                      error->message);
    g_error_free (error);
  }

  g_hash_table_unref (native_fields);
}

static void
video_upload_iface_init (gpointer g_iface,
                         gpointer iface_data)
{
  SwVideoUploadIfaceClass *klass = (SwVideoUploadIfaceClass *) g_iface;

  sw_video_upload_iface_implement_upload_video (klass,
                                                _youtube_upload_video);

}
