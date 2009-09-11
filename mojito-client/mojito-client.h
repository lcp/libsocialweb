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

#ifndef _MOJITO_CLIENT
#define _MOJITO_CLIENT

#include <glib-object.h>

#include <mojito-client/mojito-client-view.h>
#include <mojito-client/mojito-client-service.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_CLIENT mojito_client_get_type()

#define MOJITO_CLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_CLIENT, MojitoClient))

#define MOJITO_CLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_CLIENT, MojitoClientClass))

#define MOJITO_IS_CLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_CLIENT))

#define MOJITO_IS_CLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_CLIENT))

#define MOJITO_CLIENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_CLIENT, MojitoClientClass))

typedef struct {
  GObject parent;
} MojitoClient;

typedef struct {
  GObjectClass parent_class;
} MojitoClientClass;

GType mojito_client_get_type (void);

MojitoClient *mojito_client_new (void);

typedef void (*MojitoClientOpenViewCallback) (MojitoClient     *client,
                                              MojitoClientView *view,
                                              gpointer          userdata);

typedef void (*MojitoClientIsOnlineCallback) (MojitoClient *client,
                                              gboolean      online,
                                              gpointer      userdata);

void mojito_client_open_view (MojitoClient                *client,
                              GList                       *services,
                              guint                        count,
                              MojitoClientOpenViewCallback cb,
                              gpointer                     userdata);

void mojito_client_open_view_for_service (MojitoClient                 *client,
                                          const gchar                  *service_name,
                                          guint                         count,
                                          MojitoClientOpenViewCallback  cb,
                                          gpointer                      userdata);

typedef void (*MojitoClientGetServicesCallback) (MojitoClient *client,
                                                 const GList  *services,
                                                 gpointer      userdata);

void mojito_client_get_services (MojitoClient                  *client,
                                 MojitoClientGetServicesCallback cb,
                                 gpointer                       userdata);

MojitoClientService *mojito_client_get_service (MojitoClient *client,
                                                const gchar  *service_name);

void mojito_client_is_online (MojitoClient *client,
                              MojitoClientIsOnlineCallback cb,
                              gpointer userdata);

void mojito_client_hide_item (MojitoClient *client, MojitoItem *item);

G_END_DECLS

#endif /* _MOJITO_CLIENT */

