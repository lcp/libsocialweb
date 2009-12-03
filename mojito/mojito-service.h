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


#ifndef _MOJITO_SERVICE
#define _MOJITO_SERVICE

#include <glib-object.h>
#include <mojito/mojito-types.h>
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_SERVICE mojito_service_get_type()

#define MOJITO_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_SERVICE, MojitoService))

#define MOJITO_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_SERVICE, MojitoServiceClass))

#define MOJITO_IS_SERVICE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_SERVICE))

#define MOJITO_IS_SERVICE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_SERVICE))

#define MOJITO_SERVICE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_SERVICE, MojitoServiceClass))

struct _MojitoService {
  GObject parent;
};

typedef struct _MojitoServiceClass MojitoServiceClass;
struct _MojitoServiceClass {
  GObjectClass parent_class;
  /* signals */
  void (*avatar_retrieved)(MojitoService *service, const gchar *path);
  void (*caps_changed) (MojitoService *service, char **caps);

  /* vfuncs */
  /* Call before construction to see if this service can be used */
  gboolean (*ready) (MojitoServiceClass *klass); /* TODO: rename */
  const char *(*get_name) (MojitoService *service);
  void (*start) (MojitoService *service);
  /* fires ::refreshed (MojitoSet *set) signal */
  void (*refresh) (MojitoService *service);
  MojitoItem *(*get_last_item) (MojitoService *service);
  void (*update_status) (MojitoService *service, const gchar *status_message);

  const gchar ** (*get_static_caps) (MojitoService *service);
  const gchar ** (*get_dynamic_caps) (MojitoService *service);

  void (*request_avatar) (MojitoService *service);

  void (*credentials_updated) (MojitoService *service);
};

#define CAN_UPDATE_STATUS "can-update-status"
#define CAN_REQUEST_AVATAR "can-request-avatar"

GType mojito_service_get_type (void);

gboolean mojito_service_ready (MojitoServiceClass *klass);

const char *mojito_service_get_name (MojitoService *service);

void mojito_service_start (MojitoService *service);

void mojito_service_refresh (MojitoService *service);

void mojito_service_emit_refreshed (MojitoService *service, MojitoSet *set);

void mojito_service_emit_capabilities_changed (MojitoService *service, const char **caps);

void mojito_service_emit_status_updated (MojitoService *service, gboolean success);

void mojito_service_emit_avatar_retrieved (MojitoService *service,
                                           const gchar   *path);

void mojito_service_emit_user_changed (MojitoService *service);

const char * mojito_service_get_param (MojitoService *service, const char *key);

G_END_DECLS

#endif /* _MOJITO_SERVICE */

