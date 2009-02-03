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

#include "mojito-service-dummy.h"
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>
#include <mojito/mojito-utils.h>

G_DEFINE_TYPE (MojitoServiceDummy, mojito_service_dummy, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_DUMMY, MojitoServiceDummyPrivate))

typedef struct _MojitoServiceDummyPrivate MojitoServiceDummyPrivate;

struct _MojitoServiceDummyPrivate {
  gpointer dummy;
};

static const char *
get_name (MojitoService *service)
{
  return "dummy";
}

static void
update (MojitoService *service, MojitoServiceDataFunc callback, gpointer user_data)
{
  MojitoSet *set;
  MojitoItem *item;

  set = mojito_item_set_new ();

  item = mojito_item_new ();
  mojito_item_set_service (item, service);
  mojito_item_put (item, "id", "dummy-1");
  mojito_item_put (item, "title", "Dummy 1");
  mojito_item_put (item, "url", "http://burtonini.com/");
  mojito_item_take (item, "date", mojito_time_t_to_string (time (NULL)));
  mojito_set_add (set, G_OBJECT (item));
  g_object_unref (item);

  item = mojito_item_new ();
  mojito_item_set_service (item, service);
  mojito_item_put (item, "id", "dummy-2");
  mojito_item_put (item, "title", "Dummy 2");
  mojito_item_put (item, "url", "http://burtonini.com/");
  mojito_item_take (item, "date", mojito_time_t_to_string (time (NULL) - 3600));
  mojito_set_add (set, G_OBJECT (item));
  g_object_unref (item);

  callback (service, set, user_data);
}

static void
mojito_service_dummy_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  switch (property_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_service_dummy_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  switch (property_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_service_dummy_dispose (GObject *object)
{
  G_OBJECT_CLASS (mojito_service_dummy_parent_class)->dispose (object);
}

static void
mojito_service_dummy_finalize (GObject *object)
{
  G_OBJECT_CLASS (mojito_service_dummy_parent_class)->finalize (object);
}

static void
mojito_service_dummy_class_init (MojitoServiceDummyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoServiceClass *service_class = MOJITO_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoServiceDummyPrivate));

  object_class->get_property = mojito_service_dummy_get_property;
  object_class->set_property = mojito_service_dummy_set_property;
  object_class->dispose = mojito_service_dummy_dispose;
  object_class->finalize = mojito_service_dummy_finalize;

  service_class->get_name = get_name;
  service_class->update = update;
}

static void
mojito_service_dummy_init (MojitoServiceDummy *self)
{
}
