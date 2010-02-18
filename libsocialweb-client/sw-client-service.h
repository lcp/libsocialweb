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
  void (*capabilities_changed) (SwClientService *service, guint32 caps);
  void (*user_changed) (SwClientService *service);
  void (*avatar_retrieved) (SwClientService *service, gchar *path);
  void (*status_updated) (SwClientService *service, gboolean success);
} SwClientServiceClass;

GType sw_client_service_get_type (void);

#define CAN_UPDATE_STATUS "can-update-status"
#define CAN_REQUEST_AVATAR "can-request-avatar"
#define IS_CONFIGURED "is-configured"

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

const char *sw_client_service_get_name (SwClientService *service);

typedef void (*SwClientServiceQueryOpenViewCallback) (SwClientService  *query,
                                                      SwClientItemView *item_view,
                                                      gpointer          userdata);

void
sw_client_service_query_open_view (SwClientService                      *service,
                                   GHashTable                           *params,
                                   SwClientServiceQueryOpenViewCallback  cb,
                                   gpointer                              userdata);

G_END_DECLS

#endif /* _SW_CLIENT_SERVICE */
