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
#include "mojito-core.h"
#include "mojito-item.h"

G_DEFINE_ABSTRACT_TYPE (MojitoService, mojito_service, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE, MojitoServicePrivate))

typedef struct _MojitoServicePrivate MojitoServicePrivate;

struct _MojitoServicePrivate {
  MojitoCore *core;
};

enum {
  PROP_0,
  PROP_CORE
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
    case PROP_CORE:
      priv->core = g_value_dup_object (value);
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
    case PROP_CORE:
      g_value_set_object (value, priv->core);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_service_dispose (GObject *object)
{
  MojitoServicePrivate *priv = GET_PRIVATE (object);

  if (priv->core)
  {
    g_object_unref (priv->core);
    priv->core = NULL;
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

  pspec = g_param_spec_object ("core",
                               "core",
                               "The daemon's warp core",
                               MOJITO_TYPE_CORE,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_CORE, pspec);

  signals[SIGNAL_REFRESHED] =
    g_signal_new ("refreshed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  mojito_marshal_VOID__BOXED_BOXED,
                  G_TYPE_NONE, 2, MOJITO_TYPE_SET, G_TYPE_HASH_TABLE);
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
mojito_service_refresh (MojitoService *service, GHashTable *params)
{
  MojitoServiceClass *service_class = MOJITO_SERVICE_GET_CLASS (service);

  g_return_if_fail (service_class->refresh);

  service_class->refresh (service, params);
}
