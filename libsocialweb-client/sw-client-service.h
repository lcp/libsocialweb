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

#ifndef _SW_CLIENT_SERVICE
#define _SW_CLIENT_SERVICE

#include <glib-object.h>
#include <gio/gio.h>
#include <libsocialweb-client/sw-item.h>
#include <libsocialweb-client/sw-client-item-view.h>

G_BEGIN_DECLS

#define SW_CLIENT_TYPE_SERVICE sw_client_service_get_type()

#define SW_CLIENT_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_CLIENT_TYPE_SERVICE, SwClientService))

#define SW_CLIENT_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_CLIENT_TYPE_SERVICE, SwClientServiceClass))

#define SW_CLIENT_IS_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_CLIENT_TYPE_SERVICE))

#define SW_CLIENT_IS_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_CLIENT_TYPE_SERVICE))

#define SW_CLIENT_SERVICE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_CLIENT_TYPE_SERVICE, SwClientServiceClass))

typedef struct {
  GObject parent;
} SwClientService;

typedef struct {
  GObjectClass parent_class;
  void (*capabilities_changed) (SwClientService *service, const char **caps);
  void (*user_changed) (SwClientService *service);
  void (*avatar_retrieved) (SwClientService *service, gchar *path);
  void (*status_updated) (SwClientService *service, gboolean success);
} SwClientServiceClass;

GType sw_client_service_get_type (void);

/* Keep in sync with sw-service.h */
#define IS_CONFIGURED "is-configured"
#define CAN_VERIFY_CREDENTIALS "can-verify-credentials"
#define CREDENTIALS_VALID "credentials-valid"
#define CREDENTIALS_INVALID "credentials-invalid"
#define CAN_UPDATE_STATUS "can-update-status"
#define CAN_REQUEST_AVATAR "can-request-avatar"
#define CAN_GEOTAG "can-geotag"

#define HAS_UPDATE_STATUS_IFACE "has-update-status-iface"
#define HAS_AVATAR_IFACE "has-avatar-iface"
#define HAS_PHOTO_UPLOAD_IFACE "has-photo-upload-iface"
#define HAS_VIDEO_UPLOAD_IFACE "has-video-upload-iface"
#define HAS_BANISHABLE_IFACE "has-banishable-iface"
#define HAS_QUERY_IFACE "has-query-iface"

#define CAN_UPDATE_STATUS_WITH_GEOTAG "can-update-status-with-geotag"

typedef void
(*SwClientServiceGetCapabilitiesCallback) (SwClientService  *service,
                                           const char      **caps,
                                           const GError     *error,
                                           gpointer          userdata);

void
sw_client_service_get_static_capabilities (SwClientService                        *service,
                                           SwClientServiceGetCapabilitiesCallback  cb,
                                           gpointer                                userdata);

void
sw_client_service_get_dynamic_capabilities (SwClientService                        *service,
                                            SwClientServiceGetCapabilitiesCallback  cb,
                                            gpointer                                userdata);

void
sw_client_service_request_avatar (SwClientService *service);

typedef void
(*SwClientServiceUpdateStatusCallback) (SwClientService *service,
                                        const GError    *error,
                                        gpointer         userdata);

void
sw_client_service_credentials_updated (SwClientService *service);

void
sw_client_service_update_status (SwClientService                     *service,
                                 SwClientServiceUpdateStatusCallback  cb,
                                 const gchar                         *status_msg,
                                 gpointer                             userdata);
void
sw_client_service_update_status_with_fields (SwClientService                     *service,
                                             SwClientServiceUpdateStatusCallback  cb,
                                             const gchar                         *status_msg,
                                             GHashTable                          *fields,
                                             gpointer                             userdata);

gboolean
sw_client_service_upload_photo (SwClientService                      *service,
                                const char                           *filename,
                                const GHashTable                     *fields,
                                GCancellable                         *cancellable,
                                GFileProgressCallback                 progress_callback,
                                gpointer                              progress_callback_data,
                                GAsyncReadyCallback                   callback,
                                gpointer                              userdata);
gboolean
sw_client_service_upload_photo_finish (SwClientService  *service,
                                       GAsyncResult     *res,
                                       GError          **error);

gboolean
sw_client_service_upload_video (SwClientService                      *service,
                                const char                           *filename,
                                const GHashTable                     *fields,
                                GCancellable                         *cancellable,
                                GFileProgressCallback                 progress_callback,
                                gpointer                              progress_callback_data,
                                GAsyncReadyCallback                   callback,
                                gpointer                              userdata);
gboolean
sw_client_service_upload_video_finish (SwClientService  *service,
                                       GAsyncResult     *res,
                                       GError          **error);

typedef void (*SwClientServiceQueryOpenViewCallback) (SwClientService  *query,
                                                      SwClientItemView *item_view,
                                                      gpointer          userdata);

void
sw_client_service_query_open_view (SwClientService                      *service,
                                   const gchar                          *query,
                                   GHashTable                           *params,
                                   SwClientServiceQueryOpenViewCallback  cb,
                                   gpointer                              userdata);

void
sw_client_service_banishable_hide_item (SwClientService *service,
                                        const gchar     *uid);

const char *sw_client_service_get_name (SwClientService *service);

const char *sw_client_service_get_display_name (SwClientService *service);

gboolean sw_client_service_has_cap (const char **caps, const char *cap);

G_END_DECLS

#endif /* _SW_CLIENT_SERVICE */
