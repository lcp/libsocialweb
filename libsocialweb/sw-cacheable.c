/*
 * libsocialweb - social data store
 * Copyright (C) 2011 Collabora Ltd.
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

#include "sw-cacheable.h"

G_DEFINE_INTERFACE (SwCacheable, sw_cacheable, G_TYPE_OBJECT)

static void
sw_cacheable_default_init (SwCacheableInterface *iface)
{
}   

const gchar *
sw_cacheable_get_id (SwCacheable *self)
{
  SwCacheableInterface *iface = SW_CACHEABLE_GET_IFACE (self);
  g_return_val_if_fail (iface, NULL);

  return iface->get_id (self);
}

gboolean
sw_cacheable_is_ready (SwCacheable *self)
{
  SwCacheableInterface *iface = SW_CACHEABLE_GET_IFACE (self);
  g_return_val_if_fail (iface, FALSE);

  return iface->is_ready (self);
}

void
sw_cacheable_save_into_cache (SwCacheable *self, GKeyFile *keys)
{
  SwCacheableInterface *iface = SW_CACHEABLE_GET_IFACE (self);
  const gchar *group;
  g_return_if_fail (iface);

  group = sw_cacheable_get_id (self);
  if (group == NULL)
    return;

  /* Skip items that are not ready. Their properties will not be intact */
  if (!sw_cacheable_is_ready (self))
    return;

  return iface->save_into_cache (self, keys, group);
}
