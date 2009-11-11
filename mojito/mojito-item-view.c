/*
 * Mojito - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 *
 * Author: Rob Bradford <rob@linux.intel.com>
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

#include "mojito-item-view.h"
#include "mojito-item-view-ginterface.h"

#include <mojito/mojito-utils.h>
#include <mojito/mojito-core.h>

static void mojito_item_view_iface_init (gpointer g_iface, gpointer iface_data);
G_DEFINE_TYPE_WITH_CODE (MojitoItemView, mojito_item_view, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MOJITO_TYPE_ITEM_VIEW_IFACE, mojito_item_view_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_ITEM_VIEW, MojitoItemViewPrivate))

typedef struct _MojitoItemViewPrivate MojitoItemViewPrivate;

struct _MojitoItemViewPrivate {
  MojitoService *service;
  gchar *object_path;
  MojitoSet *current_items_set;
};

enum
{
  PROP_0,
  PROP_SERVICE
};

static void
mojito_item_view_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  MojitoItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_SERVICE:
      g_value_set_object (value, priv->service);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_item_view_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  MojitoItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_SERVICE:
      priv->service = g_value_dup_object (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_item_view_dispose (GObject *object)
{
  MojitoItemViewPrivate *priv = GET_PRIVATE (object);

  if (priv->service)
  {
    g_object_unref (priv->service);
    priv->service = NULL;
  }

  if (priv->current_items_set)
  {
    mojito_set_unref (priv->current_items_set);
    priv->current_items_set = NULL;
  }

  G_OBJECT_CLASS (mojito_item_view_parent_class)->dispose (object);
}

static void
mojito_item_view_finalize (GObject *object)
{
  MojitoItemViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->object_path);

  G_OBJECT_CLASS (mojito_item_view_parent_class)->finalize (object);
}

static gchar *
_make_object_path (MojitoItemView *item_view)
{
  MojitoItemViewPrivate *priv = GET_PRIVATE (item_view);
  gchar *path;
  static gint count = 0;

  path = g_strdup_printf ("/com/intel/Mojito/%s/View%d",
                          mojito_service_get_name (priv->service),
                          count);

  count++;

  return path;
}

static void
mojito_item_view_constructed (GObject *object)
{
  MojitoItemView *item_view = MOJITO_ITEM_VIEW (object);
  MojitoItemViewPrivate *priv = GET_PRIVATE (object);
  MojitoCore *core;

  core = mojito_core_dup_singleton ();

  priv->object_path = _make_object_path (item_view);
  dbus_g_connection_register_g_object (mojito_core_get_connection (core),
                                       priv->object_path,
                                       G_OBJECT (item_view));
  g_object_unref (core);

  if (G_OBJECT_CLASS (mojito_item_view_parent_class)->constructed)
    G_OBJECT_CLASS (mojito_item_view_parent_class)->constructed (object);
}

static void
mojito_item_view_class_init (MojitoItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MojitoItemViewPrivate));

  object_class->get_property = mojito_item_view_get_property;
  object_class->set_property = mojito_item_view_set_property;
  object_class->dispose = mojito_item_view_dispose;
  object_class->finalize = mojito_item_view_finalize;
  object_class->constructed = mojito_item_view_constructed;

  pspec = g_param_spec_object ("service",
                               "service",
                               "service",
                               MOJITO_TYPE_SERVICE,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_SERVICE, pspec);
}

static void
mojito_item_view_init (MojitoItemView *self)
{
  MojitoItemViewPrivate *priv = GET_PRIVATE (self);

  priv->current_items_set = mojito_set_new ();
}

static void
mojito_item_view_start (MojitoItemViewIface   *iface,
                        DBusGMethodInvocation *context)
{
  MojitoItemView *item_view = MOJITO_ITEM_VIEW (iface);

  if (MOJITO_ITEM_VIEW_GET_CLASS (iface)->start)
    MOJITO_ITEM_VIEW_GET_CLASS (iface)->start (item_view);

  mojito_item_view_iface_return_from_start (context);
}

static void
mojito_item_view_refresh (MojitoItemViewIface   *iface,
                          DBusGMethodInvocation *context)
{
  MojitoItemView *item_view = MOJITO_ITEM_VIEW (iface);

  if (MOJITO_ITEM_VIEW_GET_CLASS (iface)->refresh)
    MOJITO_ITEM_VIEW_GET_CLASS (iface)->refresh (item_view);

  mojito_item_view_iface_return_from_refresh (context);
}

static void
mojito_item_view_stop (MojitoItemViewIface   *iface,
                       DBusGMethodInvocation *context)
{
  MojitoItemView *item_view = MOJITO_ITEM_VIEW (iface);

  if (MOJITO_ITEM_VIEW_GET_CLASS (iface)->stop)
    MOJITO_ITEM_VIEW_GET_CLASS (iface)->stop (item_view);

  mojito_item_view_iface_return_from_stop (context);
}

static void
mojito_item_view_close (MojitoItemViewIface   *iface,
                        DBusGMethodInvocation *context)
{
  MojitoItemView *item_view = MOJITO_ITEM_VIEW (iface);

  if (MOJITO_ITEM_VIEW_GET_CLASS (iface)->close)
    MOJITO_ITEM_VIEW_GET_CLASS (iface)->close (item_view);

  mojito_item_view_iface_return_from_close (context);
}

static void
mojito_item_view_iface_init (gpointer g_iface,
                             gpointer iface_data)
{
  MojitoItemViewIfaceClass *klass = (MojitoItemViewIfaceClass*)g_iface;
  mojito_item_view_iface_implement_start (klass, mojito_item_view_start);
  mojito_item_view_iface_implement_refresh (klass, mojito_item_view_refresh);
  mojito_item_view_iface_implement_stop (klass, mojito_item_view_stop);
  mojito_item_view_iface_implement_close (klass, mojito_item_view_close);
}

static GValueArray *
_mojito_item_to_value_array (MojitoItem *item)
{
  GValueArray *value_array;
  time_t time;

  time = mojito_time_t_from_string (mojito_item_get (item, "date"));

  value_array = g_value_array_new (4);

  value_array = g_value_array_append (value_array, NULL);
  g_value_init (g_value_array_get_nth (value_array, 0), G_TYPE_STRING);
  g_value_set_string (g_value_array_get_nth (value_array, 0),
                      mojito_service_get_name (mojito_item_get_service (item)));

  value_array = g_value_array_append (value_array, NULL);
  g_value_init (g_value_array_get_nth (value_array, 1), G_TYPE_STRING);
  g_value_set_string (g_value_array_get_nth (value_array, 1),
                      mojito_item_get (item, "id"));

  value_array = g_value_array_append (value_array, NULL);
  g_value_init (g_value_array_get_nth (value_array, 2), G_TYPE_INT64);
  g_value_set_int64 (g_value_array_get_nth (value_array, 2),
                     time);

  value_array = g_value_array_append (value_array, NULL);
  g_value_init (g_value_array_get_nth (value_array, 3),
                dbus_g_type_get_map ("GHashTable",
                                     G_TYPE_STRING,
                                     G_TYPE_STRING));
  g_value_set_boxed (g_value_array_get_nth (value_array, 3),
                     mojito_item_peek_hash (item));

  return value_array;
}

void
mojito_item_view_add_item (MojitoItemView *item_view,
                           MojitoItem     *item)
{

}

void
mojito_item_view_update_item (MojitoItemView *item_view,
                              MojitoItem     *item)
{

}

void
mojito_item_view_remove_item (MojitoItemView *item_view,
                              MojitoItem     *item)
{

}

void
mojito_item_view_add_items (MojitoItemView *item_view,
                            GList          *items)
{
  GValueArray *value_array;
  GPtrArray *ptr_array;
  GList *l;

  ptr_array = g_ptr_array_new ();

  for (l = items; l; l = l->next)
  {
    value_array = _mojito_item_to_value_array (MOJITO_ITEM (l->data));
    g_ptr_array_add (ptr_array, value_array);
  }

  mojito_item_view_iface_emit_items_added (item_view,
                                           ptr_array);
}

void
mojito_item_view_update_items (MojitoItemView *item_view,
                               GList       *items)
{
  GValueArray *value_array;
  GPtrArray *ptr_array;
  GList *l;

  ptr_array = g_ptr_array_new ();

  for (l = items; l; l = l->next)
  {
    value_array = _mojito_item_to_value_array (MOJITO_ITEM (l->data));
    g_ptr_array_add (ptr_array, value_array);
  }

  mojito_item_view_iface_emit_items_changed (item_view,
                                             ptr_array);

}

void
mojito_item_view_remove_items (MojitoItemView *item_view,
                               GList          *items)
{
  GValueArray *value_array;
  GPtrArray *ptr_array;
  GList *l;
  MojitoItem *item;

  ptr_array = g_ptr_array_new ();

  for (l = items; l; l = l->next)
  {
    item = MOJITO_ITEM (l->data);

    value_array = g_value_array_new (2);

    value_array = g_value_array_append (value_array, NULL);
    g_value_init (g_value_array_get_nth (value_array, 0), G_TYPE_STRING);
    g_value_set_string (g_value_array_get_nth (value_array, 0),
                        mojito_service_get_name (mojito_item_get_service (item)));

    value_array = g_value_array_append (value_array, NULL);
    g_value_init (g_value_array_get_nth (value_array, 1), G_TYPE_STRING);
    g_value_set_string (g_value_array_get_nth (value_array, 1),
                        mojito_item_get (item, "id"));

    g_ptr_array_add (ptr_array, value_array);
  }

  mojito_item_view_iface_emit_items_removed (item_view,
                                             ptr_array);

}


const gchar *
mojito_item_view_get_object_path (MojitoItemView *item_view)
{
  MojitoItemViewPrivate *priv = GET_PRIVATE (item_view);

  return priv->object_path;
}

MojitoService *
mojito_item_view_get_service (MojitoItemView *item_view)
{
  MojitoItemViewPrivate *priv = GET_PRIVATE (item_view);

  return priv->service;
}

void
mojito_item_view_add_from_set (MojitoItemView *item_view,
                               MojitoSet      *set)
{
  MojitoItemViewPrivate *priv = GET_PRIVATE (item_view);
  GList *items;

  mojito_set_add_from (priv->current_items_set, set);
  items = mojito_set_as_list (set);

  mojito_item_view_add_items (item_view, items);
  g_list_free (items);
}

void
mojito_item_view_remove_from_set (MojitoItemView *item_view,
                                  MojitoSet      *set)
{
  MojitoItemViewPrivate *priv = GET_PRIVATE (item_view);
  GList *items;

  mojito_set_remove_from (priv->current_items_set, set);

  items = mojito_set_as_list (set);

  mojito_item_view_remove_items (item_view, items);
  g_list_free (items);
}

void
mojito_item_view_set_from_set (MojitoItemView *item_view,
                               MojitoSet      *set)
{
  MojitoItemViewPrivate *priv = GET_PRIVATE (item_view);
  MojitoSet *added_items, *removed_items;

  if (mojito_set_is_empty (priv->current_items_set))
  {
    mojito_item_view_add_from_set (item_view, set);
  } else {
    removed_items = mojito_set_difference (priv->current_items_set, set);
    added_items = mojito_set_difference (set, priv->current_items_set);

    mojito_item_view_remove_from_set (item_view, removed_items);
    mojito_item_view_add_from_set (item_view, added_items);

    mojito_set_unref (removed_items);
    mojito_set_unref (added_items);
  }
}
