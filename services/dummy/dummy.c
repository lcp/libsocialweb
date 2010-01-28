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

#include "dummy.h"
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-utils.h>

G_DEFINE_TYPE (SwServiceDummy, sw_service_dummy, SW_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_DUMMY, SwServiceDummyPrivate))

typedef struct _SwServiceDummyPrivate SwServiceDummyPrivate;

struct _SwServiceDummyPrivate {
  gpointer dummy;
};

static const char *
get_name (SwService *service)
{
  return "dummy";
}

static void
refresh (SwService *service)
{
  SwSet *set;
  SwItem *item;

  set = sw_item_set_new ();

  item = sw_item_new ();
  sw_item_set_service (item, service);
  sw_item_put (item, "id", "dummy-1");
  sw_item_put (item, "title", "Dummy 1");
  sw_item_put (item, "url", "http://burtonini.com/");
  sw_item_take (item, "date", sw_time_t_to_string (time (NULL)));
  sw_set_add (set, G_OBJECT (item));
  g_object_unref (item);

  item = sw_item_new ();
  sw_item_set_service (item, service);
  sw_item_put (item, "id", "dummy-2");
  sw_item_put (item, "title", "Dummy 2");
  sw_item_put (item, "url", "http://burtonini.com/");
  sw_item_take (item, "date", sw_time_t_to_string (time (NULL) - 3600));
  sw_set_add (set, G_OBJECT (item));
  g_object_unref (item);

  sw_service_emit_refreshed (service, set);
  sw_set_unref (set);
}

static void
sw_service_dummy_get_property (GObject    *object,
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
sw_service_dummy_set_property (GObject      *object,
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
sw_service_dummy_dispose (GObject *object)
{
  G_OBJECT_CLASS (sw_service_dummy_parent_class)->dispose (object);
}

static void
sw_service_dummy_finalize (GObject *object)
{
  G_OBJECT_CLASS (sw_service_dummy_parent_class)->finalize (object);
}

static void
sw_service_dummy_class_init (SwServiceDummyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceDummyPrivate));

  object_class->get_property = sw_service_dummy_get_property;
  object_class->set_property = sw_service_dummy_set_property;
  object_class->dispose = sw_service_dummy_dispose;
  object_class->finalize = sw_service_dummy_finalize;

  service_class->get_name = get_name;
  service_class->refresh = refresh;
}

static void
sw_service_dummy_init (SwServiceDummy *self)
{
}
