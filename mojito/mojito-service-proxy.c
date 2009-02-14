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

#include "mojito-service-proxy.h"

G_DEFINE_TYPE (MojitoServiceProxy, mojito_service_proxy, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_PROXY, MojitoServiceProxyPrivate))

typedef struct _MojitoServiceProxyPrivate MojitoServiceProxyPrivate;

struct _MojitoServiceProxyPrivate {
  GType type;
  MojitoService *instance;
};

enum {
  PROP_0,
  PROP_TYPE
};

static void
mojito_service_proxy_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  MojitoServiceProxyPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_TYPE:
      priv->type = g_value_get_gtype (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_service_proxy_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  MojitoServiceProxyPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_TYPE:
      g_value_set_gtype (value, priv->type);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_service_proxy_dispose (GObject *object)
{
  MojitoServiceProxyPrivate *priv = GET_PRIVATE (object);

  if (priv->instance)
  {
    g_object_unref (priv->instance);
    priv->instance = NULL;
  }

  G_OBJECT_CLASS (mojito_service_proxy_parent_class)->dispose (object);
}

static const gchar *
mojito_service_proxy_get_name (MojitoService *service)
{
  MojitoServiceProxyPrivate *priv = GET_PRIVATE (service);
  MojitoServiceClass *class;

  if (!priv->instance)
    priv->instance = g_object_new (priv->type, NULL);

  class = MOJITO_SERVICE_GET_CLASS (priv->instance);
  return class->get_name (priv->instance);
}

static void
mojito_service_proxy_update (MojitoService         *service,
                             MojitoServiceDataFunc  callback,
                             gpointer               userdata)
{
  MojitoServiceProxyPrivate *priv = GET_PRIVATE (service);
  MojitoServiceClass *class;

  if (!priv->instance)
    priv->instance = g_object_new (priv->type, NULL);

  class = MOJITO_SERVICE_GET_CLASS (priv->instance);
  return class->update (priv->instance,
                        callback,
                        userdata);
}

static void
mojito_service_proxy_class_init (MojitoServiceProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoServiceClass *service_class = MOJITO_SERVICE_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MojitoServiceProxyPrivate));

  object_class->get_property = mojito_service_proxy_get_property;
  object_class->set_property = mojito_service_proxy_set_property;
  object_class->dispose = mojito_service_proxy_dispose;

  service_class->get_name = mojito_service_proxy_get_name;
  service_class->update = mojito_service_proxy_update;

  pspec = g_param_spec_gtype ("type",
                              "type",
                              "The type we are proxying",
                              MOJITO_TYPE_SERVICE,
                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_TYPE, pspec);
}

static void
mojito_service_proxy_init (MojitoServiceProxy *self)
{
}

MojitoServiceProxy *
mojito_service_proxy_new (MojitoCore *core,
                          GType       service_type)
{
  return (MojitoServiceProxy *)g_object_new (MOJITO_TYPE_SERVICE_PROXY,
                                             "core",
                                             core,
                                             "type",
                                             service_type,
                                             NULL);
}

