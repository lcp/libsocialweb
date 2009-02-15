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
} MojitoClientServiceClass;

GType mojito_client_service_get_type (void);

typedef enum
{
  SERVICE_CAN_GET_LAST_STATUS = 1,
  SERVICE_CAN_GET_PERSONA_ICON = 1 << 2,
  SERVICE_CAN_UPDATE_STATUS = 1 << 3
} MojitoClientServiceCapabilityFlags;

typedef void
(*MojitoClientServiceGetCapabilitiesCallback) (MojitoClientService *service,
                                               guint32        caps,
                                               GError        *error,
                                               gpointer       userdata);

void
mojito_client_service_get_capabilities (MojitoClientService                       *service,
                                        MojitoClientServiceGetCapabilitiesCallback cb,
                                        gpointer                                   userdata);

typedef void 
(*MojitoClientServiceGetLastStatusCallback) (MojitoClientService *service,
                                             const gchar          *status_message,
                                             GError               *error,
                                             gpointer              userdata);

void 
mojito_client_service_get_last_status (MojitoClientService                     *service,
                                       MojitoClientServiceGetLastStatusCallback cb,
                                       gpointer                                 userdata);

typedef void 
(*MojitoClientServiceGetPersonaIconCallback) (MojitoClientService *service,
                                              const gchar         *persona_icon,
                                              GError              *error,
                                              gpointer             userdata);

void
mojito_client_service_get_persona_icon (MojitoClientService                      *service,
                                        MojitoClientServiceGetPersonaIconCallback cb,
                                        gpointer                                  userdata);

typedef void 
(*MojitoClientServiceUpdateStatusCallback) (MojitoClientService *service,
                                            gboolean             success,
                                            GError              *error,
                                            gpointer             userdata);

void
mojito_client_service_update_status (MojitoClientService                    *service,
                                     MojitoClientServiceUpdateStatusCallback cb,
                                     const gchar                            *status_msg,
                                     gpointer                                userdata);

G_END_DECLS

#endif /* _MOJITO_CLIENT_SERVICE */
