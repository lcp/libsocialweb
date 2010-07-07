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

#ifndef _SW_CLIENT
#define _SW_CLIENT

#include <glib-object.h>

#include <libsocialweb-client/sw-client-view.h>
#include <libsocialweb-client/sw-client-service.h>

G_BEGIN_DECLS

#define SW_TYPE_CLIENT sw_client_get_type()

#define SW_CLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_CLIENT, SwClient))

#define SW_CLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_CLIENT, SwClientClass))

#define SW_IS_CLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_CLIENT))

#define SW_IS_CLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_CLIENT))

#define SW_CLIENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_CLIENT, SwClientClass))

typedef struct {
  GObject parent;
} SwClient;

typedef struct {
  GObjectClass parent_class;
} SwClientClass;

GType sw_client_get_type (void);

SwClient *sw_client_new (void);

typedef void (*SwClientOpenViewCallback) (SwClient         *client,
                                          SwClientItemView *view,
                                          gpointer          userdata);

typedef void (*SwClientIsOnlineCallback) (SwClient *client,
                                          gboolean  online,
                                          gpointer  userdata);

void sw_client_open_view (SwClient                 *client,
                          GList                    *services,
                          guint                     count,
                          SwClientOpenViewCallback  cb,
                          gpointer                  userdata);

void sw_client_open_view_for_service (SwClient                 *client,
                                      const gchar              *service_name,
                                      guint                     count,
                                      SwClientOpenViewCallback  cb,
                                      gpointer                  userdata);

typedef void (*SwClientGetServicesCallback) (SwClient    *client,
                                             const GList *services,
                                             gpointer     userdata);

void sw_client_get_services (SwClient                    *client,
                             SwClientGetServicesCallback  cb,
                             gpointer                     userdata);

SwClientService *sw_client_get_service (SwClient    *client,
                                        const gchar *service_name);

void sw_client_is_online (SwClient                 *client,
                          SwClientIsOnlineCallback  cb,
                          gpointer                  userdata);


G_END_DECLS

#endif /* _SW_CLIENT */

