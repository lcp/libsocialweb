/*
 * Mojito - social data store
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

#ifndef _MOJITO_CLIENT_SERVICE
#define _MOJITO_CLIENT_SERVICE

#include <glib-object.h>
#include <mojito-client/mojito-item.h>

G_BEGIN_DECLS

#define MOJITO_CLIENT_TYPE_SERVICE mojito_client_service_get_type()

#define MOJITO_CLIENT_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_CLIENT_TYPE_SERVICE, MojitoClientService))

#define MOJITO_CLIENT_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_CLIENT_TYPE_SERVICE, MojitoClientServiceClass))

#define MOJITO_CLIENT_IS_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_CLIENT_TYPE_SERVICE))

#define MOJITO_CLIENT_IS_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_CLIENT_TYPE_SERVICE))

#define MOJITO_CLIENT_SERVICE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_CLIENT_TYPE_SERVICE, MojitoClientServiceClass))

typedef struct {
  GObject parent;
} MojitoClientService;

typedef struct {
  GObjectClass parent_class;
  void (*capabilities_changed) (MojitoClientService *service, guint32 caps);
  void (*user_changed) (MojitoClientService *service);
  void (*avatar_retrieved) (MojitoClientService *service, gchar *path);
  void (*status_updated) (MojitoClientService *service, gboolean success);
} MojitoClientServiceClass;

GType mojito_client_service_get_type (void);

#define CAN_UPDATE_STATUS "can-update-status"
#define CAN_REQUEST_AVATAR "can-request-avatar"

typedef void
(*MojitoClientServiceGetCapabilitiesCallback) (MojitoClientService *service,
                                               const char         **caps,
                                               const GError        *error,
                                               gpointer             userdata);

void
mojito_client_service_get_static_capabilities (MojitoClientService                       *service,
                                               MojitoClientServiceGetCapabilitiesCallback cb,
                                               gpointer                                   userdata);

void
mojito_client_service_get_dynamic_capabilities (MojitoClientService                       *service,
                                                MojitoClientServiceGetCapabilitiesCallback cb,
                                                gpointer                                   userdata);

void
mojito_client_service_request_avatar (MojitoClientService *service);

typedef void
(*MojitoClientServiceUpdateStatusCallback) (MojitoClientService *service,
                                            const GError        *error,
                                            gpointer             userdata);

void
mojito_client_service_update_status (MojitoClientService                    *service,
                                     MojitoClientServiceUpdateStatusCallback cb,
                                     const gchar                            *status_msg,
                                     gpointer                                userdata);

G_END_DECLS

#endif /* _MOJITO_CLIENT_SERVICE */
