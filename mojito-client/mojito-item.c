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

#include "mojito-item.h"

MojitoItem *
mojito_item_new (void)
{
  MojitoItem *item;
  
  item = g_slice_new0 (MojitoItem);
  item->refcount = 1;

  return item;
}

void
mojito_item_free (MojitoItem *item)
{
  g_free (item->service);
  g_free (item->uuid);

  if (item->props)
    g_hash_table_unref (item->props);

  g_slice_free (MojitoItem, item);
}

MojitoItem *
mojito_item_ref (MojitoItem *item)
{
  g_atomic_int_inc (&(item->refcount));
  return item;
}

void
mojito_item_unref (MojitoItem *item)
{
  if (g_atomic_int_dec_and_test (&(item->refcount)))
  {
    mojito_item_free (item);
  }
}

GType
mojito_item_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
  {
    type = g_boxed_type_register_static ("MojitoItem",
                                         (GBoxedCopyFunc)mojito_item_ref,
                                         (GBoxedFreeFunc)mojito_item_unref);
  }

  return type;
}
