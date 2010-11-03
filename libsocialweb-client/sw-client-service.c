/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
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

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gio/gio.h>

#include "sw-client-service.h"

#include <interfaces/sw-service-bindings.h>
#include <interfaces/sw-status-update-bindings.h>
#include <interfaces/sw-query-bindings.h>
#include <interfaces/sw-avatar-bindings.h>
#include <interfaces/sw-banishable-bindings.h>
#include <interfaces/sw-photo-upload-bindings.h>
#include <interfaces/sw-video-upload-bindings.h>
#include <interfaces/sw-marshals.h>

G_DEFINE_TYPE (SwClientService, sw_client_service, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_CLIENT_TYPE_SERVICE, SwClientServicePrivate))

typedef struct _SwClientServicePrivate SwClientServicePrivate;

enum
{
  AVATAR_RETRIEVED_SIGNAL,
  CAPS_CHANGED_SIGNAL,
  STATUS_UPDATED_SIGNAL,
  USER_CHANGED_SIGNAL,
  LAST_SIGNAL
};

/* We need to have one proxy per interface because dbus-glib is annoying */
typedef enum
{
  SERVICE_IFACE,
  AVATAR_IFACE,
  QUERY_IFACE,
  STATUS_UPDATE_IFACE,
  BANISHABLE_IFACE,
  PHOTO_UPLOAD_IFACE,
  VIDEO_UPLOAD_IFACE,
  LAST_IFACE
} SwServiceIface;

struct _SwClientServicePrivate {
  char *name;
  DBusGConnection *connection;
  DBusGProxy *proxies[LAST_IFACE];
  gboolean loaded_info;
  char *display_name;
};

static const gchar *interface_names[LAST_IFACE] = {
  "com.meego.libsocialweb.Service",
  "com.meego.libsocialweb.Avatar",
  "com.meego.libsocialweb.Query",
  "com.meego.libsocialweb.StatusUpdate",
  "com.meego.libsocialweb.Banishable",
  "com.meego.libsocialweb.PhotoUpload",
  "com.meego.libsocialweb.VideoUpload"
};

static guint signals[LAST_SIGNAL] = { 0 };

#define SW_CLIENT_SERVICE_NAME "com.meego.libsocialweb"
#define SW_CLIENT_SERVICE_OBJECT "/com/meego/libsocialweb/Service/%s"

#define SW_CLIENT_SERVICE_ERROR sw_client_service_error_quark()
static GQuark
sw_client_service_error_quark (void)
{
  static GQuark q = 0;
  if (q == 0) {
    q = g_quark_from_static_string ("sw-client-service-error-quark");
  }
  return q;
}

static void
sw_client_service_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  switch (property_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_client_service_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  switch (property_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_client_service_dispose (GObject *object)
{
  SwClientServicePrivate *priv = GET_PRIVATE (object);
  gint i = 0;

  if (priv->connection)
  {
    dbus_g_connection_unref (priv->connection);
    priv->connection = NULL;
  }

  for (i = 0; i < LAST_IFACE; i++)
  {
    if (priv->proxies[i])
      g_object_unref (priv->proxies[i]);
    priv->proxies[i] = NULL;
  }

  G_OBJECT_CLASS (sw_client_service_parent_class)->dispose (object);
}

static void
sw_client_service_finalize (GObject *object)
{
  SwClientServicePrivate *priv = GET_PRIVATE (object);

  g_free (priv->name);
  g_free (priv->display_name);

  G_OBJECT_CLASS (sw_client_service_parent_class)->finalize (object);
}

static void
sw_client_service_class_init (SwClientServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwClientServicePrivate));

  object_class->get_property = sw_client_service_get_property;
  object_class->set_property = sw_client_service_set_property;
  object_class->dispose = sw_client_service_dispose;
  object_class->finalize = sw_client_service_finalize;

  signals[AVATAR_RETRIEVED_SIGNAL] =
    g_signal_new ("avatar-retrieved",
                  SW_CLIENT_TYPE_SERVICE,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (SwClientServiceClass, avatar_retrieved),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  signals[CAPS_CHANGED_SIGNAL] =
    g_signal_new ("capabilities-changed",
                  SW_CLIENT_TYPE_SERVICE,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (SwClientServiceClass, capabilities_changed),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRV);

  signals[STATUS_UPDATED_SIGNAL] =
    g_signal_new ("status-updated",
                  SW_CLIENT_TYPE_SERVICE,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (SwClientServiceClass, status_updated),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  signals[USER_CHANGED_SIGNAL] =
    g_signal_new ("user-changed",
                  SW_CLIENT_TYPE_SERVICE,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (SwClientServiceClass, user_changed),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  dbus_g_object_register_marshaller (sw_marshal_VOID__INT_INT_STRING,
				     G_TYPE_NONE,
				     G_TYPE_INT,
				     G_TYPE_INT,
				     G_TYPE_STRING,
				     G_TYPE_INVALID);
}

static void
sw_client_service_init (SwClientService *self)
{
}

static void
_avatar_retrieved_cb (DBusGProxy *proxy,
                      char       *path,
                      gpointer    user_data)
{
  SwClientService *service = SW_CLIENT_SERVICE (user_data);
  g_signal_emit (service, signals[AVATAR_RETRIEVED_SIGNAL], 0, path);
}

static void
_capabilities_changed_cb (DBusGProxy *proxy,
                          char      **caps,
                          gpointer    user_data)
{
  SwClientService *service = SW_CLIENT_SERVICE (user_data);
  g_signal_emit (service, signals[CAPS_CHANGED_SIGNAL], 0, caps);
}

static void
_status_updated_cb (DBusGProxy *proxy,
                    gboolean    success,
                    gpointer    user_data)
{
  SwClientService *service = SW_CLIENT_SERVICE (user_data);
  g_signal_emit (service, signals[STATUS_UPDATED_SIGNAL], 0, success);
}

static void
_user_changed_cb (DBusGProxy *proxy,
                          gpointer    user_data)
{
  SwClientService *service = SW_CLIENT_SERVICE (user_data);
  g_signal_emit (service, signals[USER_CHANGED_SIGNAL], 0);
}

gboolean
_sw_client_service_setup_proxy_for_iface (SwClientService  *service,
                                          const gchar      *service_name,
                                          SwServiceIface    iface,
                                          GError          **error_out)
{
  SwClientServicePrivate *priv = GET_PRIVATE (service);
  GError *error = NULL;
  gchar *path;

  if (priv->proxies[iface])
    return TRUE;

  priv->connection = dbus_g_bus_get (DBUS_BUS_STARTER, &error);

  if (!priv->connection)
  {
    g_critical (G_STRLOC ": Error getting DBUS connection: %s",
                error->message);
    g_propagate_error (error_out, error);
    return FALSE;
  }

  priv->name = g_strdup (service_name);

  path = g_strdup_printf (SW_CLIENT_SERVICE_OBJECT, service_name);
  priv->proxies[iface] = dbus_g_proxy_new_for_name (priv->connection,
                                                    SW_CLIENT_SERVICE_NAME,
                                                    path,
                                                    interface_names[iface]);
  g_free (path);

  return TRUE;
}

gboolean
_sw_client_service_setup (SwClientService  *service,
                          const gchar      *service_name,
                          GError          **error_out)
{
  SwClientServicePrivate *priv = GET_PRIVATE (service);
  GError *error = NULL;

  if (!_sw_client_service_setup_proxy_for_iface (service,
                                                 service_name,
                                                 SERVICE_IFACE,
                                                 &error))
  {
    g_propagate_error (error_out, error);
    return FALSE;
  }

  if (!_sw_client_service_setup_proxy_for_iface (service,
                                                 service_name,
                                                 STATUS_UPDATE_IFACE,
                                                 &error))
  {
    g_propagate_error (error_out, error);
    return FALSE;
  }

  if (!_sw_client_service_setup_proxy_for_iface (service,
                                                 service_name,
                                                 AVATAR_IFACE,
                                                 &error))
  {
    g_propagate_error (error_out, error);
    return FALSE;
  }

  _sw_client_service_setup_proxy_for_iface (service,
					    service_name,
					    PHOTO_UPLOAD_IFACE,
					    NULL);

  dbus_g_proxy_add_signal (priv->proxies[AVATAR_IFACE],
                           "AvatarRetrieved",
                           G_TYPE_STRING,
                           G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (priv->proxies[AVATAR_IFACE],
                               "AvatarRetrieved",
                               (GCallback)_avatar_retrieved_cb,
                               service,
                               NULL);

  dbus_g_proxy_add_signal (priv->proxies[SERVICE_IFACE],
                           "CapabilitiesChanged",
                           G_TYPE_STRV,
                           NULL);
  dbus_g_proxy_connect_signal (priv->proxies[SERVICE_IFACE],
                               "CapabilitiesChanged",
                               (GCallback)_capabilities_changed_cb,
                               service,
                               NULL);

  dbus_g_proxy_add_signal (priv->proxies[STATUS_UPDATE_IFACE],
                           "StatusUpdated",
                           G_TYPE_BOOLEAN,
                           G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (priv->proxies[STATUS_UPDATE_IFACE],
                               "StatusUpdated",
                               (GCallback)_status_updated_cb,
                               service,
                               NULL);

  dbus_g_proxy_add_signal (priv->proxies[SERVICE_IFACE],
                           "UserChanged",
                           G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (priv->proxies[SERVICE_IFACE],
                               "UserChanged",
                               (GCallback)_user_changed_cb,
                               service,
                               NULL);

  if (priv->proxies[PHOTO_UPLOAD_IFACE] != NULL) {
    dbus_g_proxy_add_signal (priv->proxies[PHOTO_UPLOAD_IFACE],
			     "PhotoUploadProgress",
			     G_TYPE_INT,
			     G_TYPE_INT,
			     G_TYPE_STRING,
			     G_TYPE_INVALID);
  }

  return TRUE;
}


/* Lets use the same closure structure for them all. Allocate using slicer */
typedef struct
{
  SwClientService *service;
  GCallback cb;
  gpointer userdata;

  /* Used for photo and video upload */
  GFileProgressCallback progress_cb;
  gpointer progress_cb_data;
  guint64 filesize;
  gint opid;
  SwServiceIface iface;
  GHashTable *fields;
} SwClientServiceCallClosure;

static void
_get_capabilities_cb (DBusGProxy *proxy,
                      char      **caps,
                      GError     *error,
                      gpointer    userdata)
{
  SwClientServiceCallClosure *closure = (SwClientServiceCallClosure *)userdata;
  SwClientServiceGetCapabilitiesCallback cb;

  cb = (SwClientServiceGetCapabilitiesCallback)closure->cb;
  if (error)
  {
    g_warning (G_STRLOC ": Error getting capabilities: %s",
               error->message);
    cb (closure->service,
      NULL,
      error,
      closure->userdata);
    g_error_free (error);
  } else {
    cb (closure->service, (const char**)caps, error, closure->userdata);
    g_strfreev (caps);
  }

  g_object_unref (closure->service);
  g_slice_free (SwClientServiceCallClosure, closure);
}

void
sw_client_service_get_static_capabilities (SwClientService                        *service,
                                           SwClientServiceGetCapabilitiesCallback  cb,
                                           gpointer                                userdata)
{
  SwClientServicePrivate *priv = GET_PRIVATE (service);
  SwClientServiceCallClosure *closure;

  closure = g_slice_new0 (SwClientServiceCallClosure);
  closure->service = g_object_ref (service);
  closure->cb = (GCallback)cb;
  closure->userdata = userdata;

  com_meego_libsocialweb_Service_get_static_capabilities_async (priv->proxies[SERVICE_IFACE],
                                                                 _get_capabilities_cb,
                                                                 closure);
}

void
sw_client_service_get_dynamic_capabilities (SwClientService                        *service,
                                            SwClientServiceGetCapabilitiesCallback  cb,
                                            gpointer                                userdata)
{
  SwClientServicePrivate *priv = GET_PRIVATE (service);
  SwClientServiceCallClosure *closure;

  closure = g_slice_new0 (SwClientServiceCallClosure);
  closure->service = g_object_ref (service);
  closure->cb = (GCallback)cb;
  closure->userdata = userdata;

  com_meego_libsocialweb_Service_get_dynamic_capabilities_async (priv->proxies[SERVICE_IFACE],
                                                                  _get_capabilities_cb,
                                                                  closure);
}

static void
_update_status_cb (DBusGProxy *proxy,
                   GError     *error,
                   gpointer    userdata)
{
  SwClientServiceCallClosure *closure = (SwClientServiceCallClosure *)userdata;
  SwClientServiceUpdateStatusCallback cb;

  if (error)
  {
    g_warning (G_STRLOC ": Error updating status: %s",
               error->message);
  }

  cb = (SwClientServiceUpdateStatusCallback)closure->cb;
  cb (closure->service,
      error,
      closure->userdata);

  if (error)
    g_error_free (error);

  g_object_unref (closure->service);
  g_slice_free (SwClientServiceCallClosure, closure);
}

void
sw_client_service_update_status (SwClientService                     *service,
                                 SwClientServiceUpdateStatusCallback  cb,
                                 const gchar                         *status_msg,
                                 gpointer                             userdata)
{
  GHashTable *fields;

  fields = g_hash_table_new (g_str_hash, g_str_equal);

  sw_client_service_update_status_with_fields (service,
                                               cb,
                                               status_msg,
                                               fields,
                                               userdata);

  g_hash_table_unref (fields);
}

void
sw_client_service_update_status_with_fields (SwClientService                     *service,
                                             SwClientServiceUpdateStatusCallback  cb,
                                             const gchar                         *status_msg,
                                             GHashTable                          *fields,
                                             gpointer                             userdata)
{
  SwClientServicePrivate *priv = GET_PRIVATE (service);
  SwClientServiceCallClosure *closure;

  closure = g_slice_new0 (SwClientServiceCallClosure);
  closure->service = g_object_ref (service);
  closure->cb = (GCallback)cb;
  closure->userdata = userdata;

  com_meego_libsocialweb_StatusUpdate_update_status_async (priv->proxies[STATUS_UPDATE_IFACE],
                                                            status_msg,
                                                            fields,
                                                            _update_status_cb,
                                                            closure);
}

static void
_upload_file_progress_cb (DBusGProxy *proxy,
                           gint opid,
                           gint progress,
                           const char *error_message,
                           gpointer userdata)
{
  SwClientServiceCallClosure *closure = (SwClientServiceCallClosure *) userdata;

  if (opid != closure->opid) {
    /* Ignore, that's not our upload */
    return;
  }
  if (progress == -1) {
    g_simple_async_report_error_in_idle (G_OBJECT (closure->service),
                                         (GAsyncReadyCallback) closure->cb,
                                         closure->userdata,
                                         SW_CLIENT_SERVICE_ERROR,
                                         0,
                                         error_message);
    g_object_unref (closure->service);
    g_slice_free (SwClientServiceCallClosure, closure);
    return;
  }

  (closure->progress_cb) ((float) progress * closure->filesize,
                          closure->filesize,
                          closure->progress_cb_data);

  if (progress == 100) {
    GSimpleAsyncResult *res;
    res = g_simple_async_result_new (G_OBJECT (closure->service),
                                     (GAsyncReadyCallback) closure->cb,
                                     closure->userdata,
                                     closure->iface == PHOTO_UPLOAD_IFACE ?
                                     sw_client_service_upload_photo :
                                     sw_client_service_upload_video);
    g_simple_async_result_set_op_res_gboolean (res, TRUE);
    g_simple_async_result_complete_in_idle (res);
    g_object_unref (res);
    g_object_unref (closure->service);
    g_slice_free (SwClientServiceCallClosure, closure);
  }
}

static void
_upload_file_cb (DBusGProxy *proxy,
		 gint opid,
		 GError *error,
		 gpointer userdata)
{
  SwClientServiceCallClosure *closure = (SwClientServiceCallClosure *) userdata;

  if (error) {
    g_simple_async_report_gerror_in_idle (G_OBJECT (closure->service),
                                          (GAsyncReadyCallback) closure->cb,
                                          closure->userdata,
                                          error);
    g_object_unref (closure->service);
    g_slice_free (SwClientServiceCallClosure, closure);
    return;
  }

  closure->opid = opid;

  /* And now wait for the progress signals */
}

static void
got_file_size_cb (GObject *source_object,
                   GAsyncResult *res,
                   gpointer userdata)
{
  SwClientServiceCallClosure *closure = (SwClientServiceCallClosure *) userdata;
  SwClientServicePrivate *priv = GET_PRIVATE (closure->service);
  GFileInfo *info;
  char *filename;
  const char *signal;

  info = g_file_query_info_finish (G_FILE (source_object), res, NULL);
  if (info == NULL ||
      g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE) != G_FILE_TYPE_REGULAR) {
    g_simple_async_report_error_in_idle (G_OBJECT (closure->service),
                                         (GAsyncReadyCallback) closure->cb,
                                         closure->userdata,
                                         SW_CLIENT_SERVICE_ERROR,
                                         0,
                                         info ? "File to upload is not a regular file" :
                                         "Could not get file size information for file");
    if (info)
      g_object_unref (info);
    g_object_unref (closure->service);
    g_hash_table_unref (closure->fields);
    g_slice_free (SwClientServiceCallClosure, closure);
    return;
  }

  closure->filesize = g_file_info_get_attribute_uint64 (info,
                                                        G_FILE_ATTRIBUTE_STANDARD_SIZE);
  g_object_unref (info);

  signal = (closure->iface == PHOTO_UPLOAD_IFACE ? "PhotoUploadProgress" : "VideoUploadProgress");
  dbus_g_proxy_connect_signal (priv->proxies[closure->iface], signal,
			       G_CALLBACK (_upload_file_progress_cb), closure, NULL);

  filename = g_file_get_path (G_FILE (source_object));
  if (closure->iface == PHOTO_UPLOAD_IFACE) {
    com_meego_libsocialweb_PhotoUpload_upload_photo_async (priv->proxies[PHOTO_UPLOAD_IFACE],
							   filename,
							   closure->fields,
							   _upload_file_cb,
							   closure);
  } else {
    com_meego_libsocialweb_VideoUpload_upload_video_async (priv->proxies[VIDEO_UPLOAD_IFACE],
							   filename,
							   closure->fields,
							   _upload_file_cb,
							   closure);
  }
  g_hash_table_unref (closure->fields);
  closure->fields = NULL;

  g_free (filename);
}

gboolean
_sw_client_service_upload (SwClientService                      *service,
			   SwServiceIface                        iface,
			   const char                           *filename,
			   const GHashTable                     *fields,
			   GCancellable                         *cancellable,
			   GFileProgressCallback                 progress_callback,
			   gpointer                              progress_callback_data,
			   GAsyncReadyCallback                   callback,
			   gpointer                              userdata)
{
  SwClientServicePrivate *priv = GET_PRIVATE (service);
  SwClientServiceCallClosure *closure;
  GFile *file;

  if (!_sw_client_service_setup_proxy_for_iface (service,
                                                 priv->name,
                                                 iface,
                                                 NULL)) {
    return FALSE;
  }

  closure = g_slice_new0 (SwClientServiceCallClosure);
  closure->progress_cb = progress_callback;
  closure->progress_cb_data = progress_callback_data;
  closure->service = g_object_ref (service);
  closure->userdata = userdata;
  closure->cb = G_CALLBACK (callback);
  closure->iface = iface;
  if (fields)
    closure->fields = g_hash_table_ref ((GHashTable *) fields);
  else
    closure->fields = g_hash_table_new (g_str_hash, g_str_equal);

  file = g_file_new_for_path (filename);
  g_file_query_info_async (file,
                           G_FILE_ATTRIBUTE_STANDARD_SIZE","G_FILE_ATTRIBUTE_STANDARD_TYPE,
                           G_FILE_QUERY_INFO_NONE,
                           0,
                           NULL,
                           got_file_size_cb,
                           closure);
  g_object_unref (file);

  return TRUE;
}

gboolean
sw_client_service_upload_photo (SwClientService                      *service,
                                const char                           *filename,
                                const GHashTable                     *fields,
                                GCancellable                         *cancellable,
                                GFileProgressCallback                 progress_callback,
                                gpointer                              progress_callback_data,
                                GAsyncReadyCallback                   callback,
                                gpointer                              userdata)
{
	return _sw_client_service_upload (service,
					  PHOTO_UPLOAD_IFACE,
					  filename,
					  fields,
					  cancellable,
					  progress_callback,
					  progress_callback_data,
					  callback,
					  userdata);
}

gboolean
sw_client_service_upload_video (SwClientService                      *service,
				const char                           *filename,
				const GHashTable                     *fields,
				GCancellable                         *cancellable,
				GFileProgressCallback                 progress_callback,
				gpointer                              progress_callback_data,
				GAsyncReadyCallback                   callback,
				gpointer                              userdata)
{
	return _sw_client_service_upload (service,
					  VIDEO_UPLOAD_IFACE,
					  filename,
					  fields,
					  cancellable,
					  progress_callback,
					  progress_callback_data,
					  callback,
					  userdata);
}

gboolean
_sw_client_service_upload_finish (SwClientService  *service,
				  GAsyncResult     *res,
				  GError          **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  gboolean ret;

  g_return_val_if_fail (g_simple_async_result_is_valid (res,
							G_OBJECT (service),
							NULL), FALSE);

  ret = FALSE;

  if (g_simple_async_result_propagate_error (simple, error))
    goto out;

  ret = g_simple_async_result_get_op_res_gboolean (simple);

out:
  return ret;
}

gboolean
sw_client_service_upload_photo_finish (SwClientService  *service,
                                       GAsyncResult     *res,
                                       GError          **error)
{
  return _sw_client_service_upload_finish (service, res, error);
}

gboolean
sw_client_service_upload_video_finish (SwClientService  *service,
                                       GAsyncResult     *res,
                                       GError          **error)
{
  return _sw_client_service_upload_finish (service, res, error);
}

static void
_request_avatar_cb (DBusGProxy *proxy,
                    GError     *error,
                    gpointer    userdata)
{
  /* TODO: print the error to the console? */
}

void
sw_client_service_request_avatar (SwClientService *service)
{
  SwClientServicePrivate *priv = GET_PRIVATE (service);

  com_meego_libsocialweb_Avatar_request_avatar_async (priv->proxies[AVATAR_IFACE],
                                                       _request_avatar_cb,
                                                       NULL);
}

static void
_credentials_updated_cb (DBusGProxy *proxy, GError *error, gpointer userdata)
{
  /* TODO: print the error to the console? */
}

void
sw_client_service_credentials_updated (SwClientService *service)
{
  SwClientServicePrivate *priv = GET_PRIVATE (service);

  com_meego_libsocialweb_Service_credentials_updated_async (priv->proxies[SERVICE_IFACE],
                                                             _credentials_updated_cb,
                                                             NULL);
}

const char *
sw_client_service_get_name (SwClientService *service)
{
  return GET_PRIVATE (service)->name;
}

static void
_query_open_view_cb (DBusGProxy *proxy,
                     gchar      *view_path,
                     GError     *error,
                     gpointer    userdata)
{
  SwClientItemView *item_view = NULL;
  SwClientServiceQueryOpenViewCallback cb;
  SwClientServiceCallClosure *closure = (SwClientServiceCallClosure *)userdata;

  if (error)
  {
    g_warning (G_STRLOC ": Error callling OpenView: %s",
               error->message);
    g_error_free (error);
  } else {
    item_view = _sw_client_item_view_new_for_path (view_path);
    g_free (view_path);
  }

  cb = (SwClientServiceQueryOpenViewCallback)closure->cb;

  cb (closure->service,
      item_view,
      closure->userdata);

  g_object_unref (closure->service);
  g_slice_free (SwClientServiceCallClosure, closure);
}

void
sw_client_service_query_open_view (SwClientService                      *service,
                                   const gchar                          *query,
                                   GHashTable                           *params,
                                   SwClientServiceQueryOpenViewCallback  cb,
                                   gpointer                              userdata)
{
  SwClientServicePrivate *priv = GET_PRIVATE (service);
  SwClientServiceCallClosure *closure;
  GError *error = NULL;
  GHashTable *tmp_params = NULL;

  if (!_sw_client_service_setup_proxy_for_iface (service,
                                                 priv->name,
                                                 QUERY_IFACE,
                                                 &error))
  {
    g_critical (G_STRLOC ": Unable to setup proxy on Query interface: %s",
                error->message);
    g_clear_error (&error);
    return;
  }

  closure = g_slice_new0 (SwClientServiceCallClosure);
  closure->service = g_object_ref (service);
  closure->cb = (GCallback)cb;
  closure->userdata = userdata;

  if (!params)
  {
    tmp_params = g_hash_table_new (g_str_hash, g_str_equal);
    params = tmp_params;
  }

  com_meego_libsocialweb_Query_open_view_async (priv->proxies [QUERY_IFACE],
                                                 query,
                                                 params,
                                                 _query_open_view_cb,
                                                 closure);

  if (tmp_params)
    g_hash_table_unref (tmp_params);
}

#define GROUP "LibSocialWebService"

static void
load_info (SwClientService *service)
{
  SwClientServicePrivate *priv = GET_PRIVATE (service);

  if (!priv->loaded_info) {
    GKeyFile *keys;
    char *filename, *path;

    filename = g_strconcat (priv->name, ".keys", NULL);
    path = g_build_filename (DATADIR, "libsocialweb", "services", filename, NULL);
    g_free (filename);

    keys = g_key_file_new ();

    g_key_file_load_from_file (keys, path, G_KEY_FILE_NONE, NULL);
    g_free (path);

    priv->display_name = g_key_file_get_locale_string (keys, GROUP, "Name", NULL, NULL);

    g_key_file_free (keys);
    priv->loaded_info = TRUE;
  }
}

const char *
sw_client_service_get_display_name (SwClientService *service)
{
  SwClientServicePrivate *priv;

  g_return_val_if_fail (SW_CLIENT_IS_SERVICE (service), NULL);

  priv = GET_PRIVATE (service);

  load_info (service);

  return priv->display_name;
}

gboolean
sw_client_service_has_cap (const char **caps, const char *cap)
{
  if (!caps)
    return FALSE;

  while (*caps) {
    if (g_str_equal (*caps, cap))
      return TRUE;
    caps++;
  }

  return FALSE;
}


void
sw_client_service_banishable_hide_item (SwClientService *service,
                                        const gchar     *uid)
{
  SwClientServicePrivate *priv = GET_PRIVATE (service);
  GError *error = NULL;

  if (!_sw_client_service_setup_proxy_for_iface (service,
                                                 priv->name,
                                                 BANISHABLE_IFACE,
                                                 &error))
  {
    g_critical (G_STRLOC ": Unable to setup proxy on Banishable interface: %s",
                error->message);
    g_clear_error (&error);
    return;
  }

  com_meego_libsocialweb_Banishable_hide_item_async (priv->proxies[BANISHABLE_IFACE],
                                                      uid,
                                                      NULL,
                                                      NULL);
}
