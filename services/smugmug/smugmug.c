/*
 * libsocialweb SmugMug service support
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

#include "smugmug.h"

#define OAUTH_URL "http://api.smugmug.com/services/oauth/"
#define REST_URL "https://secure.smugmug.com/services/api/rest/1.2.2/"
#define UPLOAD_URL "http://upload.smugmug.com/photos/xmladd.mg"

static void initable_iface_init (gpointer g_iface, gpointer iface_data);
static void collections_iface_init (gpointer g_iface, gpointer iface_data);
static void photo_upload_iface_init (gpointer g_iface, gpointer iface_data);
static void video_upload_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceSmugmug, sw_service_smugmug, SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_COLLECTIONS_IFACE,
                                                collections_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_PHOTO_UPLOAD_IFACE,
                                                photo_upload_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_VIDEO_UPLOAD_IFACE,
                                                video_upload_iface_init));

struct _SwServiceSmugmugPrivate {
  const gchar *api_key;
  const gchar *api_secret;

  RestProxy *rest_proxy;
  RestProxy *upload_proxy;
  RestProxy *auth_proxy;

  gboolean configured;
  gboolean authorized;
  gboolean inited;
};

#define ALBUM_PREFIX "smugmug-"

static const ParameterNameMapping upload_params[] = {
  { "title", "Caption" },
  { "x-smugmug-hidden", "Hidden" },
  { "x-smugmug-altitude", "Altitude" },
  { "x-smugmug-latitude", "Latitude" },
  { "x-smugmug-longitude", "Longitude" },
  { "x-smugmug-keywords", "Keywords" },
  { NULL, NULL }
};

enum {
  COLLECTION = 1,
  PHOTO = 2,
  VIDEO = 4
} typedef MediaType;

static const char *
get_name (SwService *service)
{
  return "smugmug";
}

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    HAS_PHOTO_UPLOAD_IFACE,
    HAS_VIDEO_UPLOAD_IFACE,
    HAS_BANISHABLE_IFACE,
    HAS_COLLECTIONS_IFACE,

    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (SwService *service)
{
  SwServiceSmugmugPrivate *priv = SW_SERVICE_SMUGMUG (service)->priv;

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

  if (priv->authorized)
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
                 "remote SmugMug error: %s", err ?
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
  SwServiceSmugmug *self = SW_SERVICE_SMUGMUG (weak_object);
  SwService *service = SW_SERVICE (self);
  SwServiceSmugmugPrivate *priv = self->priv;

  root = node_from_call (call, &err);

  priv->authorized = (root != NULL);

  if (!priv->authorized) {
    g_assert (err != NULL);
    SW_DEBUG (SMUGMUG, "Invalid access token: %s", err->message);
    g_error_free (err);
  } else {
    rest_xml_node_unref (root);
  }

  sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
}

static void
got_tokens_cb (RestProxy *proxy, gboolean got_tokens, gpointer user_data)
{
  SwServiceSmugmug *self = (SwServiceSmugmug *) user_data;
  SwService *service = SW_SERVICE (self);
  SwServiceSmugmugPrivate *priv = self->priv;

  priv->configured = got_tokens;

  SW_DEBUG (SMUGMUG, "Got tokens: %s", got_tokens ? "yes" : "no");

  if (priv->rest_proxy != NULL)
    g_object_unref (priv->rest_proxy);

  if (priv->upload_proxy != NULL)
    g_object_unref (priv->upload_proxy);

  if (got_tokens) {
    RestProxyCall *call;
    const gchar *token = oauth_proxy_get_token ((OAuthProxy *) proxy);
    const gchar *token_secret =
      oauth_proxy_get_token_secret ((OAuthProxy *) proxy);

    priv->rest_proxy = oauth_proxy_new_with_token (priv->api_key,
                                                   priv->api_secret,
                                                   token,
                                                   token_secret,
                                                   REST_URL,
                                                   FALSE);

    priv->upload_proxy = oauth_proxy_new_with_token (priv->api_key,
                                                     priv->api_secret,
                                                     token,
                                                     token_secret,
                                                     UPLOAD_URL,
                                                     FALSE);

    call = rest_proxy_new_call (priv->rest_proxy);
    rest_proxy_call_add_param (call, "method",
                               "smugmug.auth.checkAccessToken");

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
  SwServiceSmugmug *self = (SwServiceSmugmug *) user_data;
  SwService *service = SW_SERVICE (self);
  SwServiceSmugmugPrivate *priv = self->priv;

  if (online) {
    sw_keyfob_oauth ((OAuthProxy *) priv->auth_proxy, got_tokens_cb, self);
  } else {
    priv->authorized = FALSE;
    sw_service_emit_capabilities_changed (service, get_dynamic_caps (service));
  }
}

/*
 * The credentials have been updated (or we're starting up), so fetch them from
 * the keyring.
 */
static void
refresh_credentials (SwServiceSmugmug *self)
{
  SwService *service = SW_SERVICE (self);
  SwServiceSmugmugPrivate *priv = self->priv;

  SW_DEBUG (SMUGMUG, "Credentials updated");

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
  refresh_credentials (SW_SERVICE_SMUGMUG (service));
}

static void
sw_service_smugmug_dispose (GObject *object)
{
  SwServiceSmugmugPrivate *priv = ((SwServiceSmugmug*)object)->priv;

  if (priv->auth_proxy) {
    g_object_unref (priv->auth_proxy);
    priv->auth_proxy = NULL;
  }

  if (priv->rest_proxy) {
    g_object_unref (priv->rest_proxy);
    priv->rest_proxy = NULL;
  }

  if (priv->upload_proxy) {
    g_object_unref (priv->upload_proxy);
    priv->upload_proxy = NULL;
  }

  G_OBJECT_CLASS (sw_service_smugmug_parent_class)->dispose (object);
}

static gint
_upload_file (SwServiceSmugmug *self,
              MediaType upload_type,
              const gchar *filename,
              GHashTable *extra_fields,
              RestProxyCallAsyncCallback upload_cb,
              GError **error)
{
  SwServiceSmugmugPrivate *priv = self->priv;
  RestProxyCall *call = NULL;
  RestParam *param;
  gchar *basename = NULL;
  gchar *content_type = NULL;
  gchar *bytecount = NULL;
  gchar *collection_id = NULL;
  GMappedFile *map = NULL;
  gint opid = -1;
  GChecksum *checksum = NULL;

  g_return_val_if_fail (priv->upload_proxy != NULL, -1);

  /* Open the file */
  map = g_mapped_file_new (filename, FALSE, error);
  if (*error != NULL) {
    g_warning ("Error opening file %s: %s", filename, (*error)->message);
    goto OUT;
  }

  /* Get the file information */
  basename = g_path_get_basename (filename);
  content_type = g_content_type_guess (
      filename,
      (const guchar*) g_mapped_file_get_contents (map),
      g_mapped_file_get_length (map),
      NULL);

  call = rest_proxy_new_call (priv->upload_proxy);

  bytecount = g_strdup_printf ("%lud",
                               (guint64) g_mapped_file_get_length (map));

  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (guchar *) g_mapped_file_get_contents (map),
                     g_mapped_file_get_length (map));

  rest_proxy_call_add_param (call, "MD5Sum", g_checksum_get_string (checksum));
  rest_proxy_call_add_param (call, "ResponseType", "REST");
  rest_proxy_call_add_param (call, "ByteCount", bytecount);

  collection_id = g_hash_table_lookup (extra_fields, "collection");

  if (collection_id == NULL) {
    g_set_error (error, SW_SERVICE_ERROR, SW_SERVICE_ERROR_NOT_SUPPORTED,
                 "must provide a collection ID");
    goto OUT;
  } else if (!g_str_has_prefix (collection_id, ALBUM_PREFIX) ||
             g_strstr_len (collection_id, -1, "_") == NULL) {
    g_set_error (error, SW_SERVICE_ERROR, SW_SERVICE_ERROR_NOT_SUPPORTED,
                 "collection (%s) must be in the format: %salbumkey_albumid",
                 collection_id, ALBUM_PREFIX);
    goto OUT;
  } else {
    rest_proxy_call_add_param (call, "AlbumID",
                               g_strstr_len (collection_id, -1, "_") + 1);
  }


  sw_service_map_params (upload_params, extra_fields,
                         (SwServiceSetParamFunc) rest_proxy_call_add_param,
                         call);

  g_mapped_file_ref (map);

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

  SW_DEBUG (SMUGMUG, "Uploading %s (%s)", basename, bytecount);

  rest_proxy_call_async (call,
                         upload_cb,
                         G_OBJECT (self),
                         GINT_TO_POINTER (opid),
                         NULL);

 OUT:
  g_free (basename);
  g_free (content_type);
  g_free (bytecount);
  if (checksum != NULL)
    g_checksum_free (checksum);
  if (call != NULL)
    g_object_unref (call);
  if (map != NULL)
    g_mapped_file_unref (map);

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
  const gchar *count_string;

  value_array = g_value_array_new (5);

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 0);
  g_value_init (value, G_TYPE_STRING);
  g_value_take_string (value,
                       g_strdup_printf ("%s%s_%s",
                                        ALBUM_PREFIX,
                                        (gchar *) g_hash_table_lookup (album->attrs,
                                                             "Key"),
                                        (gchar *) g_hash_table_lookup (album->attrs,
                                                             "id")));

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 1);
  g_value_init (value, G_TYPE_STRING);
  g_value_set_static_string (value,
                             g_hash_table_lookup (album->attrs, "Title"));

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 2);
  g_value_init (value, G_TYPE_STRING);
  g_value_set_static_string (value, "");

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 3);
  g_value_init (value, G_TYPE_UINT);
  g_value_set_uint (value, PHOTO | VIDEO);

  if (g_hash_table_lookup_extended (album->attrs, "ImageCount",
                                    NULL, (gpointer *) &count_string))
    count = g_ascii_strtoll (count_string, NULL, 10);

  value_array = g_value_array_append (value_array, NULL);
  value = g_value_array_get_nth (value_array, 4);
  g_value_init (value, G_TYPE_INT);
  g_value_set_int (value, count);

  g_hash_table_insert (attribs, "description",
                       g_hash_table_lookup (album->attrs, "Description"));

  g_hash_table_insert (attribs, "url",
                       g_hash_table_lookup (album->attrs, "URL"));

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

  album = rest_xml_node_find (root, "Album");

  while (album != NULL) {
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
_smugmug_collections_get_list (SwCollectionsIface *self,
                                DBusGMethodInvocation *context)
{
  SwServiceSmugmug *smugmug = SW_SERVICE_SMUGMUG (self);
  SwServiceSmugmugPrivate *priv = smugmug->priv;
  RestProxyCall *call;

  g_return_if_fail (priv->rest_proxy != NULL);

  call = rest_proxy_new_call (priv->rest_proxy);
  rest_proxy_call_add_param (call, "method", "smugmug.albums.get");
  rest_proxy_call_add_param (call, "Extras", "Description,URL,ImageCount");

  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback) _list_albums_cb,
                         (GObject *) smugmug,
                         context,
                         NULL);
}

static void
_create_album_cb (RestProxyCall *call,
    const GError  *error,
    GObject       *weak_object,
    gpointer       user_data)
{
  DBusGMethodInvocation *context = (DBusGMethodInvocation *) user_data;
  RestXmlNode *root = NULL;
  RestXmlNode *album;
  gchar *id;
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

  album = rest_xml_node_find (root, "Album");

  id = g_strdup_printf ("%s%s_%s", ALBUM_PREFIX,
                        (gchar *) g_hash_table_lookup (album->attrs, "Key"),
                        (gchar *) g_hash_table_lookup (album->attrs, "id"));

  sw_collections_iface_return_from_create (context, id);

  g_free (id);

  rest_xml_node_unref (root);
}

static void
_smugmug_collections_create (SwCollectionsIface *self,
    const gchar *collection_name,
    MediaType supported_types,
    const gchar *collection_parent,
    GHashTable *extra_parameters,
    DBusGMethodInvocation *context)
{
  SwServiceSmugmug *smugmug = SW_SERVICE_SMUGMUG (self);
  SwServiceSmugmugPrivate *priv = smugmug->priv;
  RestProxyCall *call;

  g_return_if_fail (priv->rest_proxy != NULL);

  if (strlen (collection_parent) != 0) {
    GError error = {SW_SERVICE_ERROR,
                    SW_SERVICE_ERROR_NOT_SUPPORTED,
                    "SmugMug does not support nested albums."};
    dbus_g_method_return_error (context, &error);
    return;
  }

  call = rest_proxy_new_call (priv->rest_proxy);
  rest_proxy_call_add_param (call, "method", "smugmug.albums.create");
  rest_proxy_call_add_param (call, "Title", collection_name);
  rest_proxy_call_set_method (call, "POST");

  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback) _create_album_cb,
                         (GObject *) smugmug,
                         context,
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

  album = rest_xml_node_find (root, "Album");

  collection_details = _extract_collection_details_from_xml (album);

  sw_collections_iface_return_from_get_details (context,
      collection_details);

  g_value_array_free (collection_details);

  rest_xml_node_unref (root);
}

static void
_smugmug_collections_get_details (SwCollectionsIface *self,
    const gchar *collection_id,
    DBusGMethodInvocation *context)
{
  SwServiceSmugmug *smugmug = SW_SERVICE_SMUGMUG (self);
  SwServiceSmugmugPrivate *priv = smugmug->priv;
  RestProxyCall *call;
  gchar **id;

  g_return_if_fail (priv->rest_proxy != NULL);

  if (!g_str_has_prefix(collection_id, ALBUM_PREFIX)) {
    GError *error =
      g_error_new (SW_SERVICE_ERROR,
                   SW_SERVICE_ERROR_NOT_SUPPORTED,
                   "SmugMug collection ID (%s) must start with '%s'",
                   collection_id, ALBUM_PREFIX);
    dbus_g_method_return_error (context, error);
    g_error_free (error);
    return;
  }

  id = g_strsplit (collection_id + strlen (ALBUM_PREFIX), "_", 2);

  if (g_strv_length (id) != 2) {
    g_warning ("invalid collection id");
    g_strfreev (id);
    return;
  }

  call = rest_proxy_new_call (priv->rest_proxy);
  rest_proxy_call_add_param (call, "method", "smugmug.albums.getInfo");
  rest_proxy_call_add_param (call, "AlbumID", id[1]);
  rest_proxy_call_add_param (call, "AlbumKey", id[0]);

  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback) _get_album_details_cb,
                         (GObject *) smugmug,
                         context,
                         NULL);

  g_object_unref (call);
}

static void
_smugmug_collections_get_creatable_types (SwCollectionsIface    *self,
                                              DBusGMethodInvocation *context)
{
  GArray *creatable_types = g_array_sized_new (TRUE, TRUE,
                                               sizeof (guint), 1);
  guint v =  PHOTO | VIDEO;

  g_array_append_val (creatable_types, v);

  sw_collections_iface_return_from_get_creatable_types (context,
                                                        creatable_types);

  g_array_free (creatable_types, TRUE);
}

static void
collections_iface_init (gpointer g_iface,
                        gpointer iface_data)
{
  SwCollectionsIfaceClass *klass = (SwCollectionsIfaceClass *) g_iface;

  sw_collections_iface_implement_get_list (klass,
                                           _smugmug_collections_get_list);

  sw_collections_iface_implement_create (klass,
                                         _smugmug_collections_create);

  sw_collections_iface_implement_get_details (klass,
                                              _smugmug_collections_get_details);

  sw_collections_iface_implement_get_creatable_types (klass,
                                                      _smugmug_collections_get_creatable_types);
}

/* Photo Upload Interface */

static void
_upload_photo_cb (RestProxyCall *call,
                  const GError  *error,
                  GObject       *weak_object,
                  gpointer       user_data)
{
  SwServiceSmugmug *self = SW_SERVICE_SMUGMUG (weak_object);
  int opid = GPOINTER_TO_INT (user_data);

  if (error) {
    sw_photo_upload_iface_emit_photo_upload_progress (self, opid, -1,
        error->message);
  } else {
    sw_photo_upload_iface_emit_photo_upload_progress (self, opid, 100, "");
  }
}

static void
_smugmug_upload_photo (SwPhotoUploadIface    *self,
                       const gchar           *filename,
                       GHashTable            *fields,
                       DBusGMethodInvocation *context)
{
  GError *error = NULL;
  gint opid = _upload_file (SW_SERVICE_SMUGMUG (self), PHOTO, filename, fields,
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
  SwPhotoUploadIfaceClass *klass = (SwPhotoUploadIfaceClass *) g_iface;

  sw_photo_upload_iface_implement_upload_photo (klass,
                                                _smugmug_upload_photo);

}

/* Video Upload Interface */

static void
_upload_video_cb (RestProxyCall *call,
                  const GError  *error,
                  GObject       *weak_object,
                  gpointer       user_data)
{
  SwServiceSmugmug *self = SW_SERVICE_SMUGMUG (weak_object);
  int opid = GPOINTER_TO_INT (user_data);

  if (error) {
    sw_video_upload_iface_emit_video_upload_progress (self, opid, -1,
        error->message);
  } else {
    sw_video_upload_iface_emit_video_upload_progress (self, opid, 100, "");
  }
}

static void
_smugmug_upload_video (SwVideoUploadIface    *self,
                       const gchar           *filename,
                       GHashTable            *fields,
                       DBusGMethodInvocation *context)
{
  GError *error = NULL;
  gint opid = _upload_file (SW_SERVICE_SMUGMUG (self), VIDEO, filename, fields,
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
                                                _smugmug_upload_video);

}

static void
sw_service_smugmug_class_init (SwServiceSmugmugClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceSmugmugPrivate));

  object_class->dispose = sw_service_smugmug_dispose;

  service_class->get_name = get_name;
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->credentials_updated = credentials_updated;
}

static gboolean
sw_service_smugmug_initable (GInitable     *initable,
                             GCancellable  *cancellable,
                             GError       **error)
{
  SwServiceSmugmug *self = SW_SERVICE_SMUGMUG (initable);
  SwServiceSmugmugPrivate *priv = self->priv;

  if (priv->inited)
    return TRUE;

  sw_keystore_get_key_secret ("smugmug", &priv->api_key, &priv->api_secret);

  if (priv->api_key == NULL || priv->api_secret == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API or secret key configured");
    return FALSE;
  }

  priv->inited = TRUE;

  priv->auth_proxy = oauth_proxy_new (priv->api_key, priv->api_secret,
                                      OAUTH_URL, FALSE);
  sw_online_add_notify (online_notify, self);
  refresh_credentials (self);

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface, gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_smugmug_initable;
}

static void
sw_service_smugmug_init (SwServiceSmugmug *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            SW_TYPE_SERVICE_SMUGMUG,
                                            SwServiceSmugmugPrivate);
}
