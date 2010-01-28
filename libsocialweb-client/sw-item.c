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

#include "sw-item.h"

SwItem *
sw_item_new (void)
{
  SwItem *item;

  item = g_slice_new0 (SwItem);
  item->refcount = 1;

  return item;
}

void
sw_item_free (SwItem *item)
{
  g_free (item->service);
  g_free (item->uuid);

  if (item->props)
    g_hash_table_unref (item->props);

  g_slice_free (SwItem, item);
}

SwItem *
sw_item_ref (SwItem *item)
{
  g_atomic_int_inc (&(item->refcount));
  return item;
}

void
sw_item_unref (SwItem *item)
{
  if (g_atomic_int_dec_and_test (&(item->refcount)))
  {
    sw_item_free (item);
  }
}

GType
sw_item_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
  {
    type = g_boxed_type_register_static ("SwItem",
                                         (GBoxedCopyFunc)sw_item_ref,
                                         (GBoxedFreeFunc)sw_item_unref);
  }

  return type;
}

gboolean
sw_item_is_from_cache (SwItem *item)
{
  return g_hash_table_lookup (item->props, "cached") != NULL;
}

gboolean
sw_item_has_key (SwItem  *item,
                     const gchar *key)
{
  return (g_hash_table_lookup (item->props, key) != NULL);
}

const gchar *
sw_item_get_value (SwItem  *item,
                       const gchar *key)
{
  return g_hash_table_lookup (item->props, key);
}
