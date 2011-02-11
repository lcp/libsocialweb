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

#include "smugmug.h"

#define OAUTH_URL "http://api.smugmug.com/services/oauth/"
#define REST_URL "https://secure.smugmug.com/services/api/rest/1.2.2/"
#define UPLOAD_URL "http://upload.smugmug.com/photos/xmladd.mg"

static void collections_iface_init (gpointer g_iface, gpointer iface_data);
static void photo_upload_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceSmugmug, sw_service_smugmug, SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (SW_TYPE_COLLECTIONS_IFACE,
                                                collections_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_PHOTO_UPLOAD_IFACE,
                                                photo_upload_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_SMUGMUG, SwServiceSmugmugPrivate))

struct _SwServiceSmugmugPrivate {
  const gchar *api_key;
  const gchar *api_secret;

  RestProxy *rest_proxy;
  RestProxy *upload_proxy;
  RestProxy *auth_proxy;
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
    HAS_BANISHABLE_IFACE,

    NULL
  };

  return caps;
}

static void
got_tokens_cb (RestProxy *proxy, gboolean authorised, gpointer user_data)
{
  SwServiceSmugmug *self = (SwServiceSmugmug *) user_data;
  SwServiceSmugmugPrivate *priv = self->priv;

  SW_DEBUG (SMUGMUG, "%sauthorized", authorised ? "" : "un");

  if (priv->rest_proxy != NULL)
    g_object_unref (priv->rest_proxy);

  if (priv->upload_proxy != NULL)
    g_object_unref (priv->upload_proxy);

  if (authorised) {
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
  }
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwServiceSmugmug *self = (SwServiceSmugmug *) user_data;
  SwServiceSmugmugPrivate *priv = self->priv;

  if (online) {
    sw_keyfob_oauth ((OAuthProxy *) priv->auth_proxy, got_tokens_cb, self);
  }
}

/*
 * The credentials have been updated (or we're starting up), so fetch them from
 * the keyring.
 */
static void
refresh_credentials (SwServiceSmugmug *smugmug)
{
  SW_DEBUG (SMUGMUG, "Credentials updated");

  online_notify (FALSE, (SwService *) smugmug);
  online_notify (TRUE, (SwService *) smugmug);
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
    g_warning (G_STRLOC ": error from SmugMug: %s (%d)",
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
    g_warning (G_STRLOC ": cannot make SmugMug call");
    /* TODO: display the payload if its short */
    if (node) rest_xml_node_unref (node);
    return NULL;
  }

  if (g_strcmp0 (rest_xml_node_get_attr (node, "stat"), "ok") != 0) {
    RestXmlNode *err;
    err = rest_xml_node_find (node, "err");
    if (err)
      g_warning (G_STRLOC ": cannot make SmugMug call: %s",
                 rest_xml_node_get_attr (err, "msg"));
    rest_xml_node_unref (node);
    return NULL;
  }

  return node;
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
  RestProxyCall *call;
  RestParam *param;
  gchar *basename;
  gchar *content_type;
  gchar *bytecount;
  GMappedFile *map;
  gint opid;
  GHashTableIter iter;
  gpointer key, value;
  GChecksum *checksum;

  g_return_val_if_fail (priv->upload_proxy != NULL, -1);

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

  call = rest_proxy_new_call (priv->upload_proxy);

  bytecount = g_strdup_printf ("%lud",
                               (guint64) g_mapped_file_get_length (map));

  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (guchar *) g_mapped_file_get_contents (map),
                     g_mapped_file_get_length (map));

  rest_proxy_call_add_param (call, "MD5Sum", g_checksum_get_string (checksum));
  rest_proxy_call_add_param (call, "ResponseType", "REST");
  rest_proxy_call_add_param (call, "ByteCount", bytecount);

  g_hash_table_iter_init (&iter, extra_fields);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const gchar *param_key = key;
      const gchar *param_value = value;

      if (g_strcmp0 (param_key, "collection") == 0)
        {
          const gchar *sep = g_strrstr (param_value, "_");

          if (sep == NULL)
            g_warning ("'collections' param not valid (%s)", param_value);
          else
            rest_proxy_call_add_param (call, "AlbumID", sep + 1);
        }
      else if (upload_type != VIDEO && g_strcmp0 (param_key, "title") == 0)
        {
          rest_proxy_call_add_param (call, "Caption", param_value);
        }
      else
        {
          rest_proxy_call_add_param (call, param_key, param_value);
        }
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
  g_free (bytecount);
  g_checksum_free (checksum);

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
                       g_strdup_printf ("%s_%s",
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
  RestXmlNode *root;
  RestXmlNode *album;
  GPtrArray *rv = g_ptr_array_new_with_free_func (
      (GDestroyNotify )g_value_array_free);

  root = node_from_call (call);

  g_return_if_fail (root != NULL);

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
  RestXmlNode *root = node_from_call (call);
  RestXmlNode *album = rest_xml_node_find (root, "Album");
  gchar *id;

  id = g_strdup_printf ("%s_%s",
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

}

static void
_get_album_details_cb (RestProxyCall *call,
                       const GError  *error,
                       GObject       *weak_object,
                       gpointer       user_data)
{
  DBusGMethodInvocation *context = (DBusGMethodInvocation *) user_data;
  GValueArray *collection_details;
  RestXmlNode *root = node_from_call (call);
  RestXmlNode *album = rest_xml_node_find (root, "Album");

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

  id = g_strsplit (collection_id, "_", 2);

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

static void
sw_service_smugmug_class_init (SwServiceSmugmugClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceSmugmugPrivate));

  object_class->dispose = sw_service_smugmug_dispose;

  service_class->get_name = get_name;
  service_class->get_static_caps = get_static_caps;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_smugmug_init (SwServiceSmugmug *self)
{
  SwServiceSmugmugPrivate *priv;

  priv = self->priv = GET_PRIVATE (self);

  sw_keystore_get_key_secret ("smugmug", &priv->api_key, &priv->api_secret);

  priv->auth_proxy = oauth_proxy_new (priv->api_key, priv->api_secret,
                                      OAUTH_URL, FALSE);

  sw_online_add_notify (online_notify, self);

  refresh_credentials (self);
}
