/*
 * libsocialweb Photobucket service support
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * Authors: Eitan Isaacson <eitan.isaacson@collabora.co.uk>
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
#include <libsocialweb/sw-online.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb-keystore/sw-keystore.h>

#include <rest/oauth-proxy.h>
#include <rest/rest-xml-parser.h>

#include <interfaces/sw-collections-ginterface.h>
#include <interfaces/sw-photo-upload-ginterface.h>
#include <interfaces/sw-video-upload-ginterface.h>

#include "photobucket.h"

static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void collections_iface_init (gpointer g_iface, gpointer iface_data);
static void photo_upload_iface_init (gpointer g_iface, gpointer iface_data);
static void video_upload_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServicePhotobucket, sw_service_photobucket,
                         SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_COLLECTIONS_IFACE,
                                                collections_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_PHOTO_UPLOAD_IFACE,
                                                photo_upload_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_VIDEO_UPLOAD_IFACE,
                                                video_upload_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_PHOTOBUCKET, SwServicePhotobucketPrivate))

#define ALBUM_PREFIX "photobucket-"
#define ENTRYPOINT_URL "http://api.photobucket.com/"

static const ParameterNameMapping upload_params[] = {
  { "title", "title" },
  { "description", "description" },
  { "x-photobucket-scramble", "scramble" },
  { "x-photobucket-degrees", "degrees" },
  { "x-photobucket-size", "size" },
  { "x-photobucket-read-geo-exif", "read_geo_exif" },
  { "x-photobucket-latitude", "latitude" },
  { "x-photobucket-longitude", "longitude" },
  { "x-photobucket-altitude", "altitude" },
  { "x-photobucket-compass", "compass" },
  { "x-photobucket-gps-timestamp", "gps_timestamp" },
  { NULL, NULL }
};


struct _SwServicePhotobucketPrivate {
  RestProxy *proxy;
  RestProxy *silo_proxy;
  gchar *uid;
  gboolean configured;
  gboolean inited;
};

enum {
  COLLECTION = 1,
  PHOTO = 2,
  VIDEO = 4
} typedef MediaType;

static const char *
get_name (SwService *service)
{
  return "photobucket";
}

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    HAS_PHOTO_UPLOAD_IFACE,
    HAS_VIDEO_UPLOAD_IFACE,
    HAS_BANISHABLE_IFACE,

    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServicePhotobucketPrivate *priv = SW_SERVICE_PHOTOBUCKET (service)->priv;
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

  if (priv->uid != NULL)
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
  RestXmlNode *status_node;
  const gchar *status_str = g_intern_string ("status");

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
  if (node == NULL || !g_str_equal (node->name, "response") ||
      g_hash_table_lookup (node->children, status_str) == NULL) {
    g_set_error (error, SW_SERVICE_ERROR, SW_SERVICE_ERROR_REMOTE_ERROR,
                 "malformed remote response: %s",
                 rest_proxy_call_get_payload (call));
    if (node)
      rest_xml_node_unref (node);
    return NULL;
  }

  status_node = g_hash_table_lookup (node->children, status_str);

  if (g_strcmp0 (status_node->content, "OK") != 0) {
    RestXmlNode *msg = rest_xml_node_find (node, "message");
    g_set_error (error, SW_SERVICE_ERROR, SW_SERVICE_ERROR_REMOTE_ERROR,
                 "remote Photobucket error: %s", msg->content);
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
  SwServicePhotobucket *self = SW_SERVICE_PHOTOBUCKET (weak_object);
  SwService *service = SW_SERVICE (self);
  SwServicePhotobucketPrivate *priv = self->priv;

  g_free (priv->uid);
  priv->uid = NULL;

  root = node_from_call (call, &err);

  if (err != NULL) {
    SW_DEBUG (PHOTOBUCKET, "Invalid access token: %s", err->message);
    g_error_free (err);
    return;
  }

  if (root != NULL) {
    RestXmlNode *api = rest_xml_node_find(root, "api");
    RestXmlNode *username = rest_xml_node_find(root, "username");

    if (api != NULL) {
      SW_DEBUG (PHOTOBUCKET, "silo subdomain: %s", api->content);
      rest_proxy_bind (priv->silo_proxy, api->content);
    } else {
      g_warning ("no silo subdomain");
    }

    if (username != NULL)
      priv->uid = g_strdup (username->content);
    else
      g_warning ("no username");
  }

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

static void
got_tokens_cb (RestProxy *proxy, gboolean got_tokens, gpointer user_data)
{
  SwServicePhotobucket *self = (SwServicePhotobucket *) user_data;
  SwService *service = SW_SERVICE (self);
  SwServicePhotobucketPrivate *priv = self->priv;

  priv->configured = got_tokens;

  SW_DEBUG (PHOTOBUCKET, "Got tokens: %s", got_tokens ? "yes" : "no");

  if (got_tokens) {
    RestProxyCall *call;
    OAuthProxy *oauth_proxy = OAUTH_PROXY (priv->proxy);

    oauth_proxy_set_token (OAUTH_PROXY (priv->silo_proxy),
                           oauth_proxy_get_token (oauth_proxy));
    oauth_proxy_set_token_secret (OAUTH_PROXY (priv->silo_proxy),
                                  oauth_proxy_get_token_secret (oauth_proxy));

    call = rest_proxy_new_call (priv->proxy);
    rest_proxy_call_set_function (call, "user/-/url");


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
  SwServicePhotobucket *self = (SwServicePhotobucket *) user_data;
  SwService *service = SW_SERVICE (self);
  SwServicePhotobucketPrivate *priv = self->priv;

  if (online) {
    sw_keyfob_oauth ((OAuthProxy *) priv->proxy, got_tokens_cb, self);
  } else {
    g_free (priv->uid);
    priv->uid = NULL;
    sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
  }
}

/*
 * The credentials have been updated (or we're starting up), so fetch them from
 * the keyring.
 */
static void
refresh_credentials (SwServicePhotobucket *self)
{
  SwService *service = SW_SERVICE (self);
  SwServicePhotobucketPrivate *priv = self->priv;

  SW_DEBUG (PHOTOBUCKET, "Credentials updated");

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
  refresh_credentials (SW_SERVICE_PHOTOBUCKET (service));
}

static void
sw_service_photobucket_dispose (GObject *object)
{
  SwServicePhotobucketPrivate *priv = ((SwServicePhotobucket*)object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->silo_proxy) {
    g_object_unref (priv->silo_proxy);
    priv->silo_proxy = NULL;
  }

  G_OBJECT_CLASS (sw_service_photobucket_parent_class)->dispose (object);
}

static gint
_upload_file (SwServicePhotobucket *self,
              MediaType upload_type,
              const gchar *filename,
              GHashTable *extra_fields,
              RestProxyCallAsyncCallback upload_cb,
              GError **error)
{
  SwServicePhotobucketPrivate *priv = self->priv;
  RestProxyCall *call;
  RestParam *param;
  gchar *basename;
  gchar *content_type;
  GMappedFile *map;
  gint opid = -1;
  const gchar *collection_id;
  const gchar *id;

  g_return_val_if_fail (priv->silo_proxy != NULL, -1);

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

  call = rest_proxy_new_call (priv->silo_proxy);
  rest_proxy_call_set_function (call, "album/!/upload");

  collection_id = g_hash_table_lookup (extra_fields, "collection");

  if (collection_id == NULL) {
    id = priv->uid;
  } else if (!g_str_has_prefix (collection_id, ALBUM_PREFIX)) {
    g_set_error (error, SW_SERVICE_ERROR, SW_SERVICE_ERROR_NOT_SUPPORTED,
                 "collection (%s) must be in the format: %salbumid",
                 collection_id, ALBUM_PREFIX);
    goto OUT;
  } else {
    id = collection_id + strlen (ALBUM_PREFIX);
  }

  rest_proxy_call_add_param (call, "id", id);
  rest_proxy_call_add_param (call, "type",
                             (upload_type == VIDEO) ? "video" : "image");

  sw_service_map_params (upload_params, extra_fields,
                         (SwServiceSetParamFunc) rest_proxy_call_add_param,
                         call);

  param = rest_param_new_with_owner ("uploadfile",
                                     g_mapped_file_get_contents (map),
                                     g_mapped_file_get_length (map),
                                     content_type,
                                     basename,
                                     map,
                                     (GDestroyNotify) g_mapped_file_unref);

  rest_proxy_call_add_param_full (call, param);

  rest_proxy_call_set_method (call, "POST");

  opid = sw_next_opid ();

  SW_DEBUG (PHOTOBUCKET, "Uploading %s", basename);

  rest_proxy_call_async (call,
                         upload_cb,
                         G_OBJECT (self),
                         GINT_TO_POINTER (opid),
                         NULL);

OUT:
  g_free (basename);
  g_free (content_type);
  g_object_unref (call);

  return opid;
}

/* Collections Interface */

static GValueArray *
_extract_collection_details_from_xml (RestXmlNode *album)
{
  GValueArray *value_array;
  GHashTable *attribs = g_hash_table_new (g_str_hash, g_str_equal);
  GValue *value = NULL;
  gint64 count = 0;
  const gchar *id = rest_xml_node_get_attr (album, "name");
  const gchar *title = rest_xml_node_get_attr (album, "title");
  const gchar *photo_count = rest_xml_node_get_attr (album, "photo_count");
  const gchar *video_count = rest_xml_node_get_attr (album, "video_count");
  const gchar *thumb = rest_xml_node_get_attr (album, "thumb");
  const gchar *privacy = rest_xml_node_get_attr (album, "privacy");

  const gchar *name_sep = g_strrstr (id, "/");

  value_array = g_value_array_new (5);

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 0);
  g_value_init (value, G_TYPE_STRING);
  g_value_take_string (value, g_strdup_printf ("%s%s", ALBUM_PREFIX, id));

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 1);
  g_value_init (value, G_TYPE_STRING);
  g_value_set_static_string (value, title);

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 2);
  g_value_init (value, G_TYPE_STRING);

  if (g_strstr_len (id, name_sep - id, "/") == NULL) {
    /* a "top-level" album */
    g_value_set_static_string (value, "");
  } else {
    gchar *parent_id = g_strndup (id, name_sep - id);
    g_value_take_string (value, g_strdup_printf ("%s%s", ALBUM_PREFIX,
                                                 parent_id));
    g_free (parent_id);
  }

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 3);
  g_value_init (value, G_TYPE_UINT);
  g_value_set_uint (value, PHOTO | VIDEO | COLLECTION);

  if (photo_count != NULL) {
    g_hash_table_insert (attribs, "x-photobucket-photo-count",
                         (gpointer) photo_count);
    count = g_ascii_strtoll (photo_count, NULL, 10);
  }

  if (video_count != NULL) {
    g_hash_table_insert (attribs, "x-photobucket-video-count",
                         (gpointer) video_count);
    count = count + g_ascii_strtoll (video_count, NULL, 10);
  }

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 4);
  g_value_init (value, G_TYPE_INT);
  g_value_set_int (value, count);

  if (thumb != NULL)
    g_hash_table_insert (attribs, "x-photobucket-thumb", (gpointer) thumb);

  if (privacy != NULL)
    g_hash_table_insert (attribs, "x-photobucket-privacy", (gpointer) privacy);

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 5);
  g_value_init (value, dbus_g_type_get_map ("GHashTable",
          G_TYPE_STRING,
          G_TYPE_STRING));
  g_value_take_boxed (value, attribs);

  return value_array;
}

static void
_list_albums_cb (RestProxyCall *call,
                 const GError  *error,
                 GObject       *weak_object,
                 gpointer       user_data)
{
  DBusGMethodInvocation *context = (DBusGMethodInvocation *) user_data;
  RestXmlNode *root = NULL;
  RestXmlNode *album;
  GPtrArray *rv;
  GError *err = NULL;

  if (error != NULL)
    err = g_error_new (SW_SERVICE_ERROR, SW_SERVICE_ERROR_REMOTE_ERROR,
                       "rest call failed: %s", error->message);

  if (err == NULL)
    root = node_from_call (call, &err);

  if (err != NULL) {
    dbus_g_method_return_error (context, err);
    g_error_free (err);
    if (root != NULL)
      rest_xml_node_unref (root);
    return;
  }

  rv = g_ptr_array_new_with_free_func ((GDestroyNotify )g_value_array_free);

  album = rest_xml_node_find (root, "album");

  /* The albums are nested 1 in the top level album (user album) */
  album = rest_xml_node_find (album, "album");

  while (album != NULL) {
    SW_DEBUG (PHOTOBUCKET, " name: %s",
              rest_xml_node_get_attr (album, "name"));
    GValueArray *collection_details =
            _extract_collection_details_from_xml (album);
    g_ptr_array_add (rv, collection_details);
    album = album->next;
  }

  sw_collections_iface_return_from_get_list (context, rv);

  g_ptr_array_free (rv, TRUE);
  rest_xml_node_unref (root);
}

static void
_photobucket_collections_get_list (SwCollectionsIface *self,
                                DBusGMethodInvocation *context)
{
  SwServicePhotobucket *photobucket = SW_SERVICE_PHOTOBUCKET (self);
  SwServicePhotobucketPrivate *priv = photobucket->priv;
  RestProxyCall *call;

  g_return_if_fail (priv->silo_proxy != NULL);

  call = rest_proxy_new_call (priv->silo_proxy);
  rest_proxy_call_set_function (call, "album/!");

  rest_proxy_call_add_param (call, "id", priv->uid);
  rest_proxy_call_add_param (call, "recurse", "true");
  rest_proxy_call_add_param (call, "view", "flat");
  rest_proxy_call_add_param (call, "media", "none");


  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback) _list_albums_cb,
                         (GObject *) photobucket,
                         context,
                         NULL);

  g_object_unref (call);
}

typedef struct {
  DBusGMethodInvocation *dbus_context;
  gchar *album_id;
} PhotobucketCreateAlbumCtx;

void
_create_album_ctx_free (PhotobucketCreateAlbumCtx *ctx)
{
  g_free (ctx->album_id);
  g_slice_free (PhotobucketCreateAlbumCtx, ctx);
}

static void
_create_album_cb (RestProxyCall *call,
    const GError  *error,
    GObject       *weak_object,
    gpointer       user_data)
{
  PhotobucketCreateAlbumCtx *ctx = (PhotobucketCreateAlbumCtx *) user_data;
  RestXmlNode *root = NULL;
  GError *err = NULL;

  if (error != NULL)
    err = g_error_new (SW_SERVICE_ERROR, SW_SERVICE_ERROR_REMOTE_ERROR,
                       "rest call failed: %s", error->message);

  if (err == NULL)
    root = node_from_call (call, &err);

  if (err != NULL) {
    dbus_g_method_return_error (ctx->dbus_context, err);
    g_error_free (err);
    if (root != NULL)
      rest_xml_node_unref (root);
    return;
  }

  sw_collections_iface_return_from_create (ctx->dbus_context, ctx->album_id);

  _create_album_ctx_free (ctx);
  rest_xml_node_unref (root);
}

static void
_photobucket_collections_create (SwCollectionsIface *self,
    const gchar *collection_name,
    MediaType supported_types,
    const gchar *collection_parent,
    GHashTable *extra_parameters,
    DBusGMethodInvocation *context)
{
  SwServicePhotobucket *photobucket = SW_SERVICE_PHOTOBUCKET (self);
  SwServicePhotobucketPrivate *priv = photobucket->priv;
  RestProxyCall *call;
  PhotobucketCreateAlbumCtx *ctx;
  const gchar *id;

  g_return_if_fail (priv->silo_proxy != NULL);

  if (g_strcmp0 (collection_parent, "") == 0) {
    id = priv->uid;
  } else if (!g_str_has_prefix (collection_parent, ALBUM_PREFIX)) {
    GError *error = g_error_new (SW_SERVICE_ERROR,
                                 SW_SERVICE_ERROR_NOT_SUPPORTED,
                                 "Photobucket collection ID %s must start"
                                 " with '%s'", collection_parent,
                                 ALBUM_PREFIX);

    dbus_g_method_return_error (context, error);
    g_error_free (error);
    return;
  } else {
    id = collection_parent + strlen (ALBUM_PREFIX);
  }

  call = rest_proxy_new_call (priv->silo_proxy);
  rest_proxy_call_set_function (call, "album/!");

  rest_proxy_call_add_param (call, "id", id);
  rest_proxy_call_add_param (call, "name", collection_name);
  rest_proxy_call_set_method (call, "POST");

  ctx = g_slice_new0 (PhotobucketCreateAlbumCtx);
  ctx->dbus_context = context;
  ctx->album_id = g_strdup_printf ("%s%s/%s", ALBUM_PREFIX, id,
                                   collection_name);

  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback) _create_album_cb,
                         (GObject *) photobucket,
                         ctx,
                         NULL);

  g_object_unref (call);
}

static void
_get_album_details_cb (RestProxyCall *call,
                       const GError  *error,
                       GObject       *weak_object,
                       gpointer       user_data)
{
  DBusGMethodInvocation *context = (DBusGMethodInvocation *) user_data;
  GValueArray *collection_details;
  RestXmlNode *root = NULL;
  RestXmlNode *album;
  GError *err = NULL;

  if (error != NULL)
    err = g_error_new (SW_SERVICE_ERROR, SW_SERVICE_ERROR_REMOTE_ERROR,
                       "rest call failed: %s", error->message);

  if (err == NULL)
    root = node_from_call (call, &err);

  if (err != NULL) {
    dbus_g_method_return_error (context, err);
    g_error_free (err);
    if (root != NULL)
      rest_xml_node_unref (root);
    return;
  }

  album = rest_xml_node_find (root, "album");

  collection_details = _extract_collection_details_from_xml (album);

  sw_collections_iface_return_from_get_details (context,
      collection_details);

  g_value_array_free (collection_details);

  rest_xml_node_unref (root);
}

static void
_photobucket_collections_get_details (SwCollectionsIface *self,
    const gchar *collection_id,
    DBusGMethodInvocation *context)
{
  SwServicePhotobucket *photobucket = SW_SERVICE_PHOTOBUCKET (self);
  SwServicePhotobucketPrivate *priv = photobucket->priv;
  RestProxyCall *call;

  if (!g_str_has_prefix (collection_id, ALBUM_PREFIX))
    {
      GError *error = g_error_new (SW_SERVICE_ERROR,
                                   SW_SERVICE_ERROR_NOT_SUPPORTED,
                                   "Photobucket collection ID %s must start"
                                   " with '%s'", collection_id, ALBUM_PREFIX);
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  g_return_if_fail (priv->silo_proxy != NULL);

  call = rest_proxy_new_call (priv->silo_proxy);
  rest_proxy_call_set_function (call, "album/!");

  rest_proxy_call_add_param (call, "id",
                             collection_id + strlen (ALBUM_PREFIX));
  rest_proxy_call_add_param (call, "media", "none");

  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback) _get_album_details_cb,
                         (GObject *) photobucket,
                         context,
                         NULL);

  g_object_unref (call);
}

static void
collections_iface_init (gpointer g_iface,
                        gpointer iface_data)
{
  SwCollectionsIfaceClass *klass = (SwCollectionsIfaceClass *) g_iface;

  sw_collections_iface_implement_get_list (klass,
                                           _photobucket_collections_get_list);

  sw_collections_iface_implement_create (klass,
                                         _photobucket_collections_create);

  sw_collections_iface_implement_get_details (klass,
                                              _photobucket_collections_get_details);
}

/* Photo Upload Interface */

static void
_upload_photo_cb (RestProxyCall *call,
                  const GError  *error,
                  GObject       *weak_object,
                  gpointer       user_data)
{
  SwServicePhotobucket *self = SW_SERVICE_PHOTOBUCKET (weak_object);
  int opid = GPOINTER_TO_INT (user_data);

  if (error) {
    sw_photo_upload_iface_emit_photo_upload_progress (self, opid, -1,
        error->message);
  } else {
    sw_photo_upload_iface_emit_photo_upload_progress (self, opid, 100, "");
  }
}

static void
_photobucket_upload_photo (SwPhotoUploadIface    *self,
                           const gchar           *filename,
                           GHashTable            *fields,
                           DBusGMethodInvocation *context)
{
  GError *error = NULL;
  gint opid;

  opid = _upload_file (SW_SERVICE_PHOTOBUCKET (self), PHOTO, filename,
                       fields, (RestProxyCallAsyncCallback) _upload_photo_cb,
                       &error);

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
  SwPhotoUploadIfaceClass *klass = (SwPhotoUploadIfaceClass *) g_iface;

  sw_photo_upload_iface_implement_upload_photo (klass,
                                                _photobucket_upload_photo);

}

/* Video Upload Interface */

static void
_upload_video_cb (RestProxyCall *call,
                  const GError  *error,
                  GObject       *weak_object,
                  gpointer       user_data)
{
  SwServicePhotobucket *self = SW_SERVICE_PHOTOBUCKET (weak_object);
  int opid = GPOINTER_TO_INT (user_data);

  if (error) {
    sw_video_upload_iface_emit_video_upload_progress (self, opid, -1,
        error->message);
  } else {
    sw_video_upload_iface_emit_video_upload_progress (self, opid, 100, "");
  }
}

static void
_photobucket_upload_video (SwVideoUploadIface    *self,
                           const gchar           *filename,
                           GHashTable            *fields,
                           DBusGMethodInvocation *context)
{
  GError *error = NULL;
  gint opid;

  opid = _upload_file (SW_SERVICE_PHOTOBUCKET (self), VIDEO, filename, fields,
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
  SwVideoUploadIfaceClass *klass = (SwVideoUploadIfaceClass *) g_iface;

  sw_video_upload_iface_implement_upload_video (klass,
                                                _photobucket_upload_video);

}

static void
sw_service_photobucket_class_init (SwServicePhotobucketClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServicePhotobucketPrivate));

  object_class->dispose = sw_service_photobucket_dispose;

  service_class->get_name = get_name;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->credentials_updated = credentials_updated;
}

static gboolean
sw_service_photobucket_initable (GInitable     *initable,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
  SwServicePhotobucket *self = SW_SERVICE_PHOTOBUCKET (initable);
  SwServicePhotobucketPrivate *priv = self->priv;
  const gchar *api_key;
  const gchar *api_secret;
  SoupURI *url;

  if (priv->inited)
    return TRUE;

  sw_keystore_get_key_secret ("photobucket", &api_key, &api_secret);

  if (api_key == NULL || api_secret == NULL) {
    g_set_error_literal (error, SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API or secret key configured");
    return FALSE;
  }

  priv->inited = TRUE;

  priv->proxy = oauth_proxy_new (api_key, api_secret, ENTRYPOINT_URL, FALSE);

  priv->silo_proxy = oauth_proxy_new (api_key, api_secret,
                                      "http://%s.photobucket.com/", TRUE);

  url = soup_uri_new (ENTRYPOINT_URL);

  oauth_proxy_set_signature_host (OAUTH_PROXY (priv->silo_proxy), url->host);

  sw_online_add_notify (online_notify, self);

  refresh_credentials (self);

  soup_uri_free (url);

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface, gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_photobucket_initable;
}

static void
sw_service_photobucket_init (SwServicePhotobucket *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            SW_TYPE_SERVICE_PHOTOBUCKET,
                                            SwServicePhotobucketPrivate);
}
