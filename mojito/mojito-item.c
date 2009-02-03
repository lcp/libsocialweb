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

#include <mojito/mojito-utils.h>
#include "mojito-item.h"

G_DEFINE_TYPE (MojitoItem, mojito_item, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_ITEM, MojitoItemPrivate))

struct _MojitoItemPrivate {
  MojitoService *service;
  GHashTable *hash;
  time_t cached_date;
};

static void
mojito_item_dispose (GObject *object)
{
  MojitoItem *item = MOJITO_ITEM (object);
  MojitoItemPrivate *priv = item->priv;

  if (priv->hash) {
    g_hash_table_unref (priv->hash);
    priv->hash = NULL;
  }

  if (priv->service) {
    g_object_unref (priv->service);
    priv->service = NULL;
  }

  G_OBJECT_CLASS (mojito_item_parent_class)->dispose (object);
}

static void
mojito_item_class_init (MojitoItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoItemPrivate));

  object_class->dispose = mojito_item_dispose;
}

static void
mojito_item_init (MojitoItem *self)
{
  self->priv = GET_PRIVATE (self);

  self->priv->hash = g_hash_table_new_full (NULL, NULL, NULL, g_free);
}

MojitoItem*
mojito_item_new (void)
{
  return g_object_new (MOJITO_TYPE_ITEM, NULL);
}

void
mojito_item_set_service (MojitoItem *item, MojitoService *service)
{
  g_return_if_fail (MOJITO_IS_ITEM (item));
  g_return_if_fail (MOJITO_IS_SERVICE (service));

  /* TODO: weak reference? Remember to update dispose() */
  item->priv->service = g_object_ref (service);
}

MojitoService *
mojito_item_get_service (MojitoItem *item)
{
  g_return_val_if_fail (MOJITO_IS_ITEM (item), NULL);

  return item->priv->service;
}

void
mojito_item_put (MojitoItem *item, const char *key, const char *value)
{
  g_return_if_fail (MOJITO_IS_ITEM (item));
  g_return_if_fail (key);
  g_return_if_fail (value);

  g_hash_table_insert (item->priv->hash, (gpointer)g_intern_string (key), g_strdup (value));
}

void
mojito_item_take (MojitoItem *item, const char *key, char *value)
{
  g_return_if_fail (MOJITO_IS_ITEM (item));
  g_return_if_fail (key);
  g_return_if_fail (value);

  g_hash_table_insert (item->priv->hash, (gpointer)g_intern_string (key), value);
}

const char *
mojito_item_get (MojitoItem *item, const char *key)
{
  g_return_val_if_fail (MOJITO_IS_ITEM (item), NULL);
  g_return_val_if_fail (key, NULL);

  return g_hash_table_lookup (item->priv->hash, g_intern_string (key));
}

static void
cache_date (MojitoItem *item)
{
  const char *s;

  if (item->priv->cached_date)
    return;

  s = g_hash_table_lookup (item->priv->hash, g_intern_string ("date"));
  if (!s)
    return;

  item->priv->cached_date = mojito_time_t_from_string (s);
}

int
mojito_item_compare_date_older (MojitoItem *a, MojitoItem *b)
{
  g_return_val_if_fail (MOJITO_IS_ITEM (a), 0);
  g_return_val_if_fail (MOJITO_IS_ITEM (b), 0);

  cache_date (a);
  cache_date (b);

  return a->priv->cached_date - b->priv->cached_date;
}

int
mojito_item_compare_date_newer (MojitoItem *a, MojitoItem *b)
{
  g_return_val_if_fail (MOJITO_IS_ITEM (a), 0);
  g_return_val_if_fail (MOJITO_IS_ITEM (b), 0);

  cache_date (a);
  cache_date (b);

  return b->priv->cached_date - a->priv->cached_date;
}

void
mojito_item_dump (MojitoItem *item)
{
  GHashTableIter iter;
  const char *key, *value;

  g_return_if_fail (MOJITO_IS_ITEM (item));

  g_printerr ("MojitoItem %p\n", item);
  g_hash_table_iter_init (&iter, item->priv->hash);
  while (g_hash_table_iter_next (&iter, (gpointer)&key, (gpointer)&value)) {
    g_printerr (" %s=%s\n", key, value);
  }
}

static guint
item_hash (gconstpointer key)
{
  const MojitoItem *item = key;
  return g_str_hash (g_hash_table_lookup (item->priv->hash, g_intern_string ("id")));
}

gboolean
item_equal (gconstpointer a, gconstpointer b)
{
  const MojitoItem *item_a = a;
  const MojitoItem *item_b = b;

  return g_str_equal (g_hash_table_lookup (item_a->priv->hash, g_intern_string ("id")),
                      g_hash_table_lookup (item_b->priv->hash, g_intern_string ("id")));
}

MojitoSet *
mojito_item_set_new (void)
{
  return mojito_set_new_full (item_hash, item_equal);
}

GHashTable *
mojito_item_peek_hash (MojitoItem *item)
{
  g_return_val_if_fail (MOJITO_IS_ITEM (item), NULL);

  return item->priv->hash;
}
