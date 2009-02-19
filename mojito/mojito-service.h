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
#include <mojito/mojito-core.h>
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

typedef void (*MojitoServiceDataFunc) (MojitoService *service, MojitoSet *set, gpointer user_data);

typedef struct _MojitoServiceClass MojitoServiceClass;
struct _MojitoServiceClass {
  GObjectClass parent_class;
  const char *(*get_name) (MojitoService *service);
  void (*update) (MojitoService *service, MojitoServiceDataFunc callback, gpointer user_data);
  MojitoItem *(*get_last_item) (MojitoService *service);
  gchar *(*get_persona_icon) (MojitoService *service);
  gboolean (*update_status) (MojitoService *service, const gchar *status_message);
  guint32 (*get_capabilities) (MojitoService *service);
};

typedef enum
{
  SERVICE_CAN_GET_LAST_ITEM = 1,
  SERVICE_CAN_GET_PERSONA_ICON = 1 << 2,
  SERVICE_CAN_UPDATE_STATUS = 1 << 3
} MojitoServiceCapabilityFlags;

GType mojito_service_get_type (void);

MojitoCore *mojito_service_get_core (MojitoService *service);

const char *mojito_service_get_name (MojitoService *service);

/* Please update yourself */
void mojito_service_update (MojitoService *service, MojitoServiceDataFunc callback, gpointer user_data);

G_END_DECLS

#endif /* _MOJITO_SERVICE */

