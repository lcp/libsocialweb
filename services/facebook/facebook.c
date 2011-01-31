/*
 * libsocialweb Facebook service support
 *
 * Copyright (C) 2010 Novell, Inc.
 * Copyright (C) Collabora Ltd.
 *
 * Authors: Gary Ching-Pang Lin <glin@novell.com>
 *          Thomas Thurman <thomas.thurman@collabora.co.uk>
 *          Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *          Danielle Madeley <danielle.madeley@collabora.co.uk>
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
#include <string.h>

#include "facebook.h"
#include "facebook-util.h"
#include "facebook-item-view.h"

#include <json-glib/json-glib.h>
#include <rest/oauth2-proxy.h>

#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb/sw-online.h>
#include <libsocialweb/sw-client-monitor.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <interfaces/sw-avatar-ginterface.h>
#include <interfaces/sw-status-update-ginterface.h>
#include <interfaces/sw-photo-upload-ginterface.h>
#include <interfaces/sw-video-upload-ginterface.h>
#include <interfaces/sw-query-ginterface.h>

static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void query_iface_init (gpointer g_iface, gpointer iface_data);
static void avatar_iface_init (gpointer g_iface, gpointer iface_data);
static void status_update_iface_init (gpointer g_iface, gpointer iface_data);
static void photo_upload_iface_init (gpointer g_iface, gpointer iface_data);
static void video_upload_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceFacebook,
                         sw_service_facebook,
                         SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_QUERY_IFACE,
                                                query_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_AVATAR_IFACE,
                                                avatar_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_STATUS_UPDATE_IFACE,
                                                status_update_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_PHOTO_UPLOAD_IFACE,
                                                photo_upload_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_VIDEO_UPLOAD_IFACE,
                                                video_upload_iface_init));

#define GET_PRIVATE(o) (((SwServiceFacebook *) o)->priv)

enum {
  UPLOAD_PHOTO,
  UPLOAD_VIDEO
} typedef UploadType;

struct _SwServiceFacebookPrivate {
  gboolean inited;
  gboolean online;
  RestProxy *proxy;
  RestProxy *video_proxy;
  char *uid;
  char *display_name;
  char *profile_url;
  char *pic_square;
};

static GList *service_list;
static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    HAS_UPDATE_STATUS_IFACE,
    HAS_AVATAR_IFACE,
    HAS_BANISHABLE_IFACE,
    HAS_QUERY_IFACE,
    HAS_PHOTO_UPLOAD_IFACE,
    HAS_VIDEO_UPLOAD_IFACE,

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
  SwServiceFacebookPrivate *priv = GET_PRIVATE (service);
  static const char *offline_caps[] = {
    IS_CONFIGURED,
    NULL
  };
  static const char *full_caps[] = {
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    IS_CONFIGURED,
    CREDENTIALS_VALID,
    NULL
  };
  static const char *no_caps[] = { NULL };

  if (!priv->uid)
    return no_caps;
  else if (priv->online)
    return full_caps;
  else
    return offline_caps;
}

static void
clear_user_info (SwServiceFacebook *facebook)
{
  SwServiceFacebookPrivate *priv = facebook->priv;

  g_free (priv->uid);
  priv->uid = NULL;
  g_free (priv->display_name);
  priv->display_name = NULL;
  g_free (priv->profile_url);
  priv->profile_url = NULL;
  g_free (priv->pic_square);
  priv->pic_square = NULL;
}

static gboolean
_facebook_extract_user_info (SwServiceFacebook *facebook,
                             JsonNode *node)
{
  SwServiceFacebookPrivate *priv = facebook->priv;

  clear_user_info (facebook);
  priv->uid = get_child_node_value(node, "id");
  priv->display_name = get_child_node_value(node, "name");
  priv->profile_url = get_child_node_value(node, "link");

  /* we don't currently use the profile url for anything important, so if it's
   * missing, we don't particularly care */
  if (!priv->uid || !priv->display_name) {
    clear_user_info (facebook);
    return FALSE;
  }

  priv->pic_square = build_picture_url (priv->proxy, priv->uid, FB_PICTURE_SIZE_SQUARE);
  return TRUE;
}

static void
got_user_info_cb (RestProxyCall *call,
                  const GError  *error,
                  GObject       *weak_object,
                  gpointer       userdata)
{
  SwService *service = SW_SERVICE (weak_object);
  SwServiceFacebook *facebook = SW_SERVICE_FACEBOOK (service);
  JsonNode *node;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  node = json_node_from_call (call, NULL);
  if (!node)
    return;

  _facebook_extract_user_info (facebook, node);

  json_node_free (node);

  sw_service_emit_capabilities_changed
    (service, get_dynamic_caps (service));
}

static void
got_tokens_cb (RestProxy *proxy, gboolean authorised, gpointer user_data)
{
  SwServiceFacebook *facebook = SW_SERVICE_FACEBOOK (user_data);
  SwServiceFacebookPrivate *priv = facebook->priv;
  RestProxyCall *call;

  if (authorised) {
    call = rest_proxy_new_call (priv->proxy);
    rest_proxy_call_set_function (call, "me");
    rest_proxy_call_async (call,
                           (RestProxyCallAsyncCallback)got_user_info_cb,
                           (GObject*)facebook,
                           NULL,
                           NULL);
    g_object_unref (call);
  }
}

static const char *
sw_service_facebook_get_name (SwService *service)
{
  return "facebook";
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwServiceFacebook *service = (SwServiceFacebook *) user_data;
  SwServiceFacebookPrivate *priv = service->priv;
  priv->online = online;

  if (online) {
    sw_keyfob_oauth2 ((OAuth2Proxy *)priv->proxy, got_tokens_cb, service);
  } else {
    sw_service_emit_capabilities_changed ((SwService *)service,
                                          get_dynamic_caps ((SwService*)service));
    g_free (priv->uid);
    priv->uid = NULL;
  }
}

static void
_credentials_updated_func (gpointer data, gpointer userdata)
{
  SwService *service = SW_SERVICE (data);
  SwServiceFacebookPrivate *priv = SW_SERVICE_FACEBOOK (service)->priv;

  online_notify (FALSE, service);
  /* Clean up pic_square to prevent avatar retrieving */
  if (priv->pic_square){
     g_free (priv->pic_square);
     priv->pic_square = NULL;
  }
  online_notify (TRUE, service);

  sw_service_emit_user_changed (service);
  sw_service_emit_capabilities_changed ((SwService *)service,
                                        get_dynamic_caps (service));
}

static void
credentials_updated (SwService *service)
{
  /* If we're online, force a reconnect to fetch new credentials */
  if (sw_is_online ()) {
    g_list_foreach (service_list, _credentials_updated_func, NULL);
  }
}

static void
sw_service_facebook_dispose (GObject *object)
{
  SwServiceFacebookPrivate *priv = SW_SERVICE_FACEBOOK (object)->priv;

  service_list = g_list_remove (service_list, SW_SERVICE_FACEBOOK (object));

  sw_online_remove_notify (online_notify, object);

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->video_proxy) {
    g_object_unref (priv->video_proxy);
    priv->video_proxy = NULL;
  }

  G_OBJECT_CLASS (sw_service_facebook_parent_class)->dispose (object);
}

static void
sw_service_facebook_finalize (GObject *object)
{
  SwServiceFacebookPrivate *priv = SW_SERVICE_FACEBOOK (object)->priv;

  g_free (priv->uid);
  g_free (priv->display_name);
  g_free (priv->profile_url);
  g_free (priv->pic_square);

  G_OBJECT_CLASS (sw_service_facebook_parent_class)->finalize (object);
}

static void
sw_service_facebook_class_init (SwServiceFacebookClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceFacebookPrivate));

  object_class->dispose = sw_service_facebook_dispose;
  object_class->finalize = sw_service_facebook_finalize;

  service_class->get_name = sw_service_facebook_get_name;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_facebook_init (SwServiceFacebook *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self), SW_TYPE_SERVICE_FACEBOOK,
      SwServiceFacebookPrivate);

  self->priv->inited = FALSE;
  service_list = g_list_append (service_list, self);
}

/* Initable interface */
static gboolean
sw_service_facebook_initable (GInitable     *initable,
                              GCancellable  *cancellable,
                              GError       **error)
{
  SwServiceFacebook *facebook = SW_SERVICE_FACEBOOK (initable);
  SwServiceFacebookPrivate *priv = facebook->priv;
  GKeyFile *keys;
  const char *key = NULL, *secret = NULL;
  char *auth_url = NULL, *graph_url = NULL, *video_url = NULL;
  char *filename;
  gboolean rv = FALSE;

  if (priv->inited)
    return TRUE;

  /* we don't actually care about secret */
  sw_keystore_get_key_secret ("facebook", &key, &secret);
  if (key == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }

  keys = g_key_file_new ();

  filename = g_build_filename("libsocialweb",
                              "services",
                              "facebook.keys",
                              NULL);

  g_key_file_load_from_data_dirs (keys,
                                  filename,
                                  NULL, G_KEY_FILE_NONE, NULL);

  g_free (filename);

  auth_url = g_key_file_get_string (keys,
                                    "OAuth2",
                                    "AuthEndpoint",
                                    NULL);

  graph_url = g_key_file_get_string (keys,
                                     "OAuth2",
                                     "BaseUri",
                                     NULL);

  video_url = g_key_file_get_string (keys,
                                     "OAuth2",
                                     "BaseVideoUri",
                                     NULL);

  if (auth_url == NULL) {
    g_warning ("Auth URL not found in keys file");
    goto out;
  }

  if (graph_url == NULL) {
    g_warning ("Graph URL not found in keys file");
    goto out;
  }

  if (video_url == NULL) {
    g_warning ("Video upload URL not found in keys file");
    goto out;
  }

  priv->proxy = oauth2_proxy_new (key,
                                  auth_url,
                                  graph_url,
                                  FALSE);

  priv->video_proxy = rest_proxy_new (video_url, FALSE);

  if (sw_is_online ()) {
    online_notify (TRUE, facebook);
  }
  sw_online_add_notify (online_notify, facebook);

  priv->inited = TRUE;

  rv = TRUE;

 out:
  g_free (auth_url);
  g_free (video_url);
  g_free (graph_url);
  g_key_file_free (keys);

  return rv;
}

static void
initable_iface_init (gpointer g_iface, gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_facebook_initable;
}

/* Query interface */
static void
_facebook_query_open_view (SwQueryIface          *self,
                           const gchar           *query,
                           GHashTable            *params,
                           DBusGMethodInvocation *context)
{
  SwServiceFacebookPrivate *priv = GET_PRIVATE (self);
  SwItemView *item_view;
  const gchar *object_path;

  g_debug ("query = '%s'", query);

  item_view = g_object_new (SW_TYPE_FACEBOOK_ITEM_VIEW,
                            "service", self,
                            "proxy", priv->proxy,
                            "query", query,
                            "params", params,
                            NULL);
  object_path = sw_item_view_get_object_path (item_view);

  /* Ensure the object gets disposed when the client goes away */
  sw_client_monitor_add (dbus_g_method_get_sender (context),
                         (GObject *) item_view);

  sw_query_iface_return_from_open_view (context, object_path);
}

static void
query_iface_init (gpointer g_iface, gpointer iface_data)
{
  SwQueryIfaceClass *klass = (SwQueryIfaceClass*) g_iface;

  sw_query_iface_implement_open_view (klass, _facebook_query_open_view);
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
_facebook_avatar_request_avatar (SwAvatarIface     *self,
                                 DBusGMethodInvocation *context)
{
  SwServiceFacebookPrivate *priv = GET_PRIVATE (self);

  if (priv->pic_square) {
    sw_web_download_image_async (priv->pic_square,
                                 _requested_avatar_downloaded_cb,
                                 self);
  }

  sw_avatar_iface_return_from_request_avatar (context);
}

static void
avatar_iface_init (gpointer g_iface,
                   gpointer iface_data)
{
  SwAvatarIfaceClass *klass = (SwAvatarIfaceClass *)g_iface;

  sw_avatar_iface_implement_request_avatar (klass, _facebook_avatar_request_avatar);
}

/* Status Update interface */
static void
_update_status_cb (RestProxyCall *call,
                   const GError  *error,
                   GObject       *weak_object,
                   gpointer       userdata)
{
  if (error) {
    g_critical (G_STRLOC ": Error updating status: %s",
                error->message);
    sw_status_update_iface_emit_status_updated (weak_object, FALSE);
  } else {
//    SW_DEBUG (FACEBOOK, G_STRLOC ": Status updated.");
    sw_status_update_iface_emit_status_updated (weak_object, TRUE);
  }

}

static void
_facebook_status_update_update_status (SwStatusUpdateIface   *self,
                                       const gchar           *msg,
                                       GHashTable            *fields,
                                       DBusGMethodInvocation *context)
{
  SwServiceFacebook *facebook = (SwServiceFacebook *)self;
  SwServiceFacebookPrivate *priv = facebook->priv;
  RestProxyCall *call;

  if (!priv->proxy)
    return;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_function (call, "me/feed");
  rest_proxy_call_add_param (call, "message", msg);
  rest_proxy_call_set_method (call, "POST");

  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback)_update_status_cb,
                         (GObject *)facebook,
                         NULL,
                         NULL);
  sw_status_update_iface_return_from_update_status (context);
}

static void
status_update_iface_init (gpointer g_iface,
                          gpointer iface_data)
{
  SwStatusUpdateIfaceClass *klass = (SwStatusUpdateIfaceClass*)g_iface;

  sw_status_update_iface_implement_update_status (klass,
                                                      _facebook_status_update_update_status);
}

static gint
_upload_file (SwServiceFacebook *self,
    UploadType upload_type,
    const gchar *filename,
    GHashTable *extra_fields,
    RestProxyCallAsyncCallback upload_cb,
    GError **error)
{
  SwServiceFacebookPrivate *priv = self->priv;
  RestProxyCall *call;
  RestParam *param;
  gchar *basename;
  gchar *content_type;
  GMappedFile *map;
  gint opid;
  GHashTableIter iter;
  gpointer key, value;

  g_return_val_if_fail (priv->proxy != NULL, -1);

  /* Open the file */
  map = g_mapped_file_new (filename, FALSE, error);
  if (*error != NULL) {
    g_warning ("Error opening file %s: %s", filename, (*error)->message);
    return -1;
  }

  /* Get the file information */
  basename = g_path_get_basename (filename);
  content_type = g_content_type_guess (
      filename,
      (const guchar*) g_mapped_file_get_contents (map),
      g_mapped_file_get_length (map),
      NULL);

  if (upload_type != UPLOAD_VIDEO)
    {
      call = rest_proxy_new_call (priv->proxy);
      rest_proxy_call_set_function (call, "me/photos");
    }
  else
    {
      call = rest_proxy_new_call (priv->video_proxy);
      rest_proxy_call_set_function (call,
          "restserver.php?method=video.upload");
      rest_proxy_call_add_param (call, "access_token",
          oauth2_proxy_get_access_token (OAUTH2_PROXY (priv->proxy)));
      rest_proxy_call_add_param (call, "format", "json");
    }

  g_hash_table_iter_init (&iter, extra_fields);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const gchar *param_key = key;
      const gchar *param_value = value;

      if (upload_type != UPLOAD_VIDEO && g_strcmp0 (param_key, "title") == 0)
        param_key = "message";

      rest_proxy_call_add_param (call, param_key, param_value);
    }

  param = rest_param_new_with_owner (basename,
                                     g_mapped_file_get_contents (map),
                                     g_mapped_file_get_length (map),
                                     content_type,
                                     basename,
                                     map,
                                     (GDestroyNotify)g_mapped_file_unref);

  rest_proxy_call_add_param_full (call, param);

  rest_proxy_call_set_method (call, "POST");

  opid = sw_next_opid ();

  rest_proxy_call_async (call,
                         upload_cb,
                         G_OBJECT (self),
                         GINT_TO_POINTER (opid),
                         NULL);

  g_free (basename);
  g_free (content_type);

  return opid;
}

static void
_upload_photo_cb (RestProxyCall *call,
                  const GError  *error,
                  GObject       *weak_object,
                  gpointer       user_data)
{
  SwServiceFacebook *facebook = SW_SERVICE_FACEBOOK (weak_object);
  int opid = GPOINTER_TO_INT (user_data);

  if (error) {
    sw_photo_upload_iface_emit_photo_upload_progress (facebook, opid, -1,
        error->message);
  } else {
    sw_photo_upload_iface_emit_photo_upload_progress (facebook, opid, 100, "");
  }
}

static void
_facebook_photo_upload_upload_photo (SwPhotoUploadIface    *self,
                                     const gchar           *filename,
                                     GHashTable            *fields,
                                     DBusGMethodInvocation *context)
{
  SwServiceFacebook *facebook = SW_SERVICE_FACEBOOK (self);
  gint opid;
  GError *error = NULL;

  opid = _upload_file (facebook, UPLOAD_PHOTO, filename, fields,
      (RestProxyCallAsyncCallback) _upload_photo_cb, &error);

  if (error) {
    dbus_g_method_return_error (context, error);
    g_error_free (error);
    return;
  }

  sw_photo_upload_iface_return_from_upload_photo (context, opid);
}

static void
photo_upload_iface_init (gpointer g_iface,
                         gpointer iface_data)
{
  SwPhotoUploadIfaceClass *klass = (SwPhotoUploadIfaceClass*)g_iface;

  sw_photo_upload_iface_implement_upload_photo (
          klass, _facebook_photo_upload_upload_photo);
}

static void
_upload_video_cb (RestProxyCall *call,
                  const GError  *error,
                  GObject       *weak_object,
                  gpointer       user_data)
{
  SwServiceFacebook *facebook = SW_SERVICE_FACEBOOK (weak_object);
  int opid = GPOINTER_TO_INT (user_data);

  if (error) {
    sw_video_upload_iface_emit_video_upload_progress (facebook, opid, -1,
        error->message);
  } else {
    sw_video_upload_iface_emit_video_upload_progress (facebook, opid, 100, "");
  }
}

static void
_facebook_video_upload_upload_video (SwVideoUploadIface    *self,
                                     const gchar           *filename,
                                     GHashTable            *fields,
                                     DBusGMethodInvocation *context)
{
  SwServiceFacebook *facebook = SW_SERVICE_FACEBOOK (self);
  gint opid;
  GError *error = NULL;

  opid = _upload_file (facebook, UPLOAD_VIDEO, filename, fields,
      (RestProxyCallAsyncCallback) _upload_video_cb, &error);

  if (error) {
    dbus_g_method_return_error (context, error);
    g_error_free (error);
    return;
  }

  sw_video_upload_iface_return_from_upload_video (context, opid);
}

static void
video_upload_iface_init (gpointer g_iface,
                         gpointer iface_data)
{
  SwVideoUploadIfaceClass *klass = (SwVideoUploadIfaceClass*)g_iface;

  sw_video_upload_iface_implement_upload_video (
          klass, _facebook_video_upload_upload_video);
}

const char *
sw_service_facebook_get_uid (SwServiceFacebook *self)
{
  g_return_val_if_fail (SW_IS_SERVICE_FACEBOOK (self), NULL);

  return GET_PRIVATE (self)->uid;
}
