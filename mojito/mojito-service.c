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

#include "mojito-service.h"
#include "mojito-marshals.h"
#include "mojito-item.h"
#include "mojito-service-ginterface.h"
static void service_iface_init (gpointer g_iface, gpointer iface_data);
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (MojitoService, mojito_service, G_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (MOJITO_TYPE_SERVICE_IFACE,
                                                         service_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE, MojitoServicePrivate))

typedef struct _MojitoServicePrivate MojitoServicePrivate;

struct _MojitoServicePrivate {
  GHashTable *params;
};

enum {
  PROP_0,
  PROP_PARAMS
};

enum {
  SIGNAL_REFRESHED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

static void
mojito_service_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  MojitoServicePrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
  case PROP_PARAMS:
    if (priv->params)
      g_hash_table_unref (priv->params);
    priv->params = g_value_dup_boxed (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_service_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  MojitoServicePrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
  case PROP_PARAMS:
    g_value_set_boxed (value, priv->params);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_service_dispose (GObject *object)
{
  MojitoServicePrivate *priv = GET_PRIVATE (object);

  if (priv->params) {
    g_hash_table_unref (priv->params);
    priv->params = NULL;
  }

  G_OBJECT_CLASS (mojito_service_parent_class)->dispose (object);
}

static void
mojito_service_class_init (MojitoServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MojitoServicePrivate));

  object_class->get_property = mojito_service_get_property;
  object_class->set_property = mojito_service_set_property;
  object_class->dispose = mojito_service_dispose;

  /* TODO: make services define gobject properties for each property they support */
  pspec = g_param_spec_boxed ("params", "params",
                               "The service parameters",
                              G_TYPE_HASH_TABLE,
                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_PARAMS, pspec);

  signals[SIGNAL_REFRESHED] =
    g_signal_new ("refreshed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, MOJITO_TYPE_SET);
}

static void
mojito_service_init (MojitoService *self)
{
}

const char *
mojito_service_get_name (MojitoService *service)
{
  MojitoServiceClass *service_class = MOJITO_SERVICE_GET_CLASS (service);

  g_return_val_if_fail (service_class->get_name, NULL);

  return service_class->get_name (service);
}

void
mojito_service_start (MojitoService *service)
{
  MojitoServiceClass *service_class = MOJITO_SERVICE_GET_CLASS (service);

  g_return_if_fail (service_class->start);

  service_class->start (service);
}

void
mojito_service_refresh (MojitoService *service)
{
  MojitoServiceClass *service_class = MOJITO_SERVICE_GET_CLASS (service);

  g_return_if_fail (service_class->refresh);

  service_class->refresh (service);
}

void
mojito_service_emit_refreshed (MojitoService *service, MojitoSet *set)
{
  g_return_if_fail (MOJITO_IS_SERVICE (service));

  g_signal_emit (service, signals[SIGNAL_REFRESHED], 0, set ? mojito_set_ref (set) : NULL);
}

void
mojito_service_emit_capabilities_changed (MojitoService *service, const char **caps)
{
  g_return_if_fail (MOJITO_IS_SERVICE (service));

  mojito_service_iface_emit_capabilities_changed (MOJITO_SERVICE_IFACE (service), caps);
}

void
mojito_service_emit_status_updated (MojitoService *service, gboolean success)
{
  g_return_if_fail (MOJITO_IS_SERVICE (service));

  mojito_service_iface_emit_status_updated (service, success);
}

void
mojito_service_emit_avatar_retrieved (MojitoService *service,
                                      const gchar   *path)
{
  g_return_if_fail (MOJITO_IS_SERVICE (service));

  mojito_service_iface_emit_avatar_retrieved (service, path);
}

void
mojito_service_emit_user_changed (MojitoService *service)
{
  g_return_if_fail (MOJITO_IS_SERVICE (service));

  mojito_service_iface_emit_user_changed (service);
}


static void
service_request_avatar (MojitoServiceIface    *self,
                        DBusGMethodInvocation *context)
{
  MojitoServiceClass *service_class;

  service_class = MOJITO_SERVICE_GET_CLASS (self);

  if (service_class->request_avatar)
  {
    service_class->request_avatar ((MojitoService *)self);
  }

  mojito_service_iface_return_from_request_avatar (context);
}

static void
service_update_status (MojitoServiceIface    *self,
                       const gchar           *status_message,
                       DBusGMethodInvocation *context)
{
  MojitoServiceClass *service_class;

  service_class = MOJITO_SERVICE_GET_CLASS (self);

  if (service_class->update_status)
  {
    service_class->update_status ((MojitoService *)self, status_message);
  }

  mojito_service_iface_return_from_update_status (context);
}

static void
service_get_static_caps (MojitoServiceIface *self, DBusGMethodInvocation *context)
{
  MojitoServiceClass *service_class;
  const char **caps = NULL;

  service_class = MOJITO_SERVICE_GET_CLASS (self);

  if (service_class->get_static_caps)
    caps = service_class->get_static_caps ((MojitoService *)self);

  mojito_service_iface_return_from_get_static_capabilities (context, caps);
}

static void
service_get_dynamic_caps (MojitoServiceIface *self, DBusGMethodInvocation *context)
{
  MojitoServiceClass *service_class;
  const char **caps = NULL;

  service_class = MOJITO_SERVICE_GET_CLASS (self);

  if (service_class->get_dynamic_caps)
    caps = service_class->get_dynamic_caps ((MojitoService *)self);

  mojito_service_iface_return_from_get_dynamic_capabilities (context, caps);
}

/*
 * Convenience function for subclasses to lookup a paramter
 */
const char *
mojito_service_get_param (MojitoService *service, const char *key)
{
  MojitoServicePrivate *priv;

  g_return_val_if_fail (MOJITO_IS_SERVICE (service), NULL);
  g_return_val_if_fail (key, NULL);

  priv = GET_PRIVATE (service);

  if (priv->params)
    return g_hash_table_lookup (priv->params, key);
  else
    return NULL;
}

static void
service_iface_init (gpointer g_iface,
                    gpointer iface_data)
{
  MojitoServiceIfaceClass *klass = (MojitoServiceIfaceClass *)g_iface;

  mojito_service_iface_implement_update_status (klass,
                                                service_update_status);
  mojito_service_iface_implement_get_static_capabilities (klass,
                                                   service_get_static_caps);
  mojito_service_iface_implement_get_dynamic_capabilities (klass,
                                                   service_get_dynamic_caps);
  mojito_service_iface_implement_request_avatar (klass,
                                                 service_request_avatar);
}
