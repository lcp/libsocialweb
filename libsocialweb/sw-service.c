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

#include "sw-service.h"
#include "sw-marshals.h"
#include "sw-item.h"
#include "sw-service-ginterface.h"

static void service_iface_init (gpointer g_iface, gpointer iface_data);
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (SwService, sw_service, G_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (SW_TYPE_SERVICE_IFACE,
                                                         service_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE, SwServicePrivate))

typedef struct _SwServicePrivate SwServicePrivate;

struct _SwServicePrivate {
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

GQuark
sw_service_error_quark (void)
{
  return g_quark_from_static_string ("sw-service-error-quark");
}

static void
sw_service_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  SwServicePrivate *priv = GET_PRIVATE (object);

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
sw_service_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  SwServicePrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
  case PROP_PARAMS:
    g_value_set_boxed (value, priv->params);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_service_dispose (GObject *object)
{
  SwServicePrivate *priv = GET_PRIVATE (object);

  if (priv->params) {
    g_hash_table_unref (priv->params);
    priv->params = NULL;
  }

  G_OBJECT_CLASS (sw_service_parent_class)->dispose (object);
}

static void
sw_service_class_init (SwServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwServicePrivate));

  object_class->get_property = sw_service_get_property;
  object_class->set_property = sw_service_set_property;
  object_class->dispose = sw_service_dispose;

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
                  G_TYPE_NONE, 1, SW_TYPE_SET);
}

static void
sw_service_init (SwService *self)
{
}

gboolean
sw_service_ready (SwServiceClass *klass)
{
  g_return_val_if_fail (SW_IS_SERVICE_CLASS (klass), FALSE);

  if (klass->ready)
    return klass->ready (klass);
  else
    return TRUE;
}

const char *
sw_service_get_name (SwService *service)
{
  SwServiceClass *service_class = SW_SERVICE_GET_CLASS (service);

  g_return_val_if_fail (service_class->get_name, NULL);

  return service_class->get_name (service);
}

void
sw_service_start (SwService *service)
{
  SwServiceClass *service_class = SW_SERVICE_GET_CLASS (service);

  g_return_if_fail (service_class->start);

  service_class->start (service);
}

void
sw_service_refresh (SwService *service)
{
  SwServiceClass *service_class = SW_SERVICE_GET_CLASS (service);

  g_return_if_fail (service_class->refresh);

  service_class->refresh (service);
}

void
sw_service_emit_refreshed (SwService *service, SwSet *set)
{
  g_return_if_fail (SW_IS_SERVICE (service));

  g_signal_emit (service, signals[SIGNAL_REFRESHED], 0, set ? sw_set_ref (set) : NULL);
}

void
sw_service_emit_capabilities_changed (SwService *service, const char **caps)
{
  g_return_if_fail (SW_IS_SERVICE (service));

  sw_service_iface_emit_capabilities_changed (SW_SERVICE_IFACE (service), caps);
}

void
sw_service_emit_avatar_retrieved (SwService *service,
                                      const gchar   *path)
{
  g_return_if_fail (SW_IS_SERVICE (service));

  sw_service_iface_emit_avatar_retrieved (service, path);
}

void
sw_service_emit_user_changed (SwService *service)
{
  g_return_if_fail (SW_IS_SERVICE (service));

  sw_service_iface_emit_user_changed (service);
}


static void
service_request_avatar (SwServiceIface    *self,
                        DBusGMethodInvocation *context)
{
  SwServiceClass *service_class;

  service_class = SW_SERVICE_GET_CLASS (self);

  if (service_class->request_avatar)
  {
    service_class->request_avatar ((SwService *)self);
  }

  sw_service_iface_return_from_request_avatar (context);
}

static void
service_get_static_caps (SwServiceIface *self, DBusGMethodInvocation *context)
{
  SwServiceClass *service_class;
  const char **caps = NULL;

  service_class = SW_SERVICE_GET_CLASS (self);

  if (service_class->get_static_caps)
    caps = service_class->get_static_caps ((SwService *)self);

  sw_service_iface_return_from_get_static_capabilities (context, caps);
}

static void
service_get_dynamic_caps (SwServiceIface *self, DBusGMethodInvocation *context)
{
  SwServiceClass *service_class;
  const char **caps = NULL;

  service_class = SW_SERVICE_GET_CLASS (self);

  if (service_class->get_dynamic_caps)
    caps = service_class->get_dynamic_caps ((SwService *)self);

  sw_service_iface_return_from_get_dynamic_capabilities (context, caps);
}

static void
service_credentials_updated (SwServiceIface *self, DBusGMethodInvocation *context)
{
  SwServiceClass *service_class;

  service_class = SW_SERVICE_GET_CLASS (self);

  if (service_class->credentials_updated)
    service_class->credentials_updated ((SwService *)self);

  sw_service_iface_return_from_credentials_updated (context);
}

/*
 * Convenience function for subclasses to lookup a paramter
 */
const char *
sw_service_get_param (SwService *service, const char *key)
{
  SwServicePrivate *priv;

  g_return_val_if_fail (SW_IS_SERVICE (service), NULL);
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
  SwServiceIfaceClass *klass = (SwServiceIfaceClass *)g_iface;

  sw_service_iface_implement_get_static_capabilities (klass,
                                                   service_get_static_caps);
  sw_service_iface_implement_get_dynamic_capabilities (klass,
                                                   service_get_dynamic_caps);
  sw_service_iface_implement_request_avatar (klass,
                                                 service_request_avatar);
  sw_service_iface_implement_credentials_updated (klass,
                                                      service_credentials_updated);
}
