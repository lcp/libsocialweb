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
#include "sw-banishable-ginterface.h"
#include "sw-banned.h"

static void service_iface_init (gpointer g_iface, gpointer iface_data);
static void banishable_iface_init (gpointer g_iface, gpointer iface_data);
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (SwService, sw_service, G_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (SW_TYPE_SERVICE_IFACE,
                                                         service_iface_init)
                                  G_IMPLEMENT_INTERFACE (SW_TYPE_BANISHABLE_IFACE,
                                                         banishable_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE, SwServicePrivate))

typedef struct _SwServicePrivate SwServicePrivate;

struct _SwServicePrivate {
  GHashTable *banned_uids;
};

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
  switch (property_id) {
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
  switch (property_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_service_dispose (GObject *object)
{
  SwServicePrivate *priv = GET_PRIVATE (object);

  if (priv->banned_uids)
  {
    g_hash_table_unref (priv->banned_uids);
    priv->banned_uids = NULL;
  }

  G_OBJECT_CLASS (sw_service_parent_class)->dispose (object);
}

static void
sw_service_constructed (GObject *object)
{
  SwService *self = SW_SERVICE (object);
  SwServicePrivate *priv = GET_PRIVATE (self);

  priv->banned_uids = sw_ban_load (sw_service_get_name (self));

  if (G_OBJECT_CLASS (sw_service_parent_class)->constructed)
    G_OBJECT_CLASS (sw_service_parent_class)->constructed (object);
}

static void
sw_service_class_init (SwServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServicePrivate));

  object_class->get_property = sw_service_get_property;
  object_class->set_property = sw_service_set_property;
  object_class->dispose = sw_service_dispose;
  object_class->constructed = sw_service_constructed;

  dbus_g_error_domain_register (SW_SERVICE_ERROR, NULL, SW_TYPE_SERVICE_ERROR);
}

static void
sw_service_init (SwService *self)
{
}

const char *
sw_service_get_name (SwService *service)
{
  SwServiceClass *service_class = SW_SERVICE_GET_CLASS (service);

  g_return_val_if_fail (service_class->get_name, NULL);

  return service_class->get_name (service);
}

void
sw_service_emit_capabilities_changed (SwService   *service,
                                      const char **caps)
{
  g_return_if_fail (SW_IS_SERVICE (service));

  sw_service_iface_emit_capabilities_changed (SW_SERVICE_IFACE (service), caps);
}

void
sw_service_emit_user_changed (SwService *service)
{
  g_return_if_fail (SW_IS_SERVICE (service));

  sw_service_iface_emit_user_changed (service);
}

static void
service_get_static_caps (SwServiceIface        *self,
                         DBusGMethodInvocation *context)
{
  SwServiceClass *service_class;
  const char **caps = NULL;

  service_class = SW_SERVICE_GET_CLASS (self);

  if (service_class->get_static_caps)
    caps = service_class->get_static_caps ((SwService *)self);

  sw_service_iface_return_from_get_static_capabilities (context, caps);
}

static void
service_get_dynamic_caps (SwServiceIface        *self,
                          DBusGMethodInvocation *context)
{
  SwServiceClass *service_class;
  const char **caps = NULL;

  service_class = SW_SERVICE_GET_CLASS (self);

  if (service_class->get_dynamic_caps)
    caps = service_class->get_dynamic_caps ((SwService *)self);

  sw_service_iface_return_from_get_dynamic_capabilities (context, caps);
}

static void
service_credentials_updated (SwServiceIface        *self,
                             DBusGMethodInvocation *context)
{
  SwServiceClass *service_class;

  service_class = SW_SERVICE_GET_CLASS (self);

  if (service_class->credentials_updated)
    service_class->credentials_updated ((SwService *)self);

  sw_service_iface_return_from_credentials_updated (context);
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
  sw_service_iface_implement_credentials_updated (klass,
                                                  service_credentials_updated);
}

static void
banishable_hide_item (SwBanishableIface     *self,
                      const gchar           *uid,
                      DBusGMethodInvocation *context)
{
  SwServicePrivate *priv = GET_PRIVATE (self);

  g_hash_table_insert (priv->banned_uids,
                       g_strdup (uid),
                       GINT_TO_POINTER (42));

  sw_banishable_iface_emit_item_hidden (self, uid);
  sw_ban_save (sw_service_get_name (SW_SERVICE (self)), priv->banned_uids);

  sw_banishable_iface_return_from_hide_item (context);
}

static void
banishable_iface_init (gpointer g_iface,
                       gpointer iface_data)
{
  SwBanishableIfaceClass *klass = (SwBanishableIfaceClass *)g_iface;

  sw_banishable_iface_implement_hide_item (klass,
                                           banishable_hide_item);
}

gboolean
sw_service_is_uid_banned (SwService   *service,
                          const gchar *uid)
{
  SwServicePrivate *priv = GET_PRIVATE (service);

  if (g_hash_table_lookup (priv->banned_uids, uid))
  {
    return TRUE;
  } else {
    return FALSE;
  }
}
