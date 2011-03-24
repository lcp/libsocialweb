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


#ifndef _SW_CACHEABLE
#define _SW_CACHEABLE

#include <glib-object.h>
#include <libsocialweb/sw-types.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-enum-types.h>

G_BEGIN_DECLS

#define SW_TYPE_CACHEABLE            (sw_cacheable_get_type ())
#define SW_CACHEABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    SW_TYPE_CACHEABLE, SwCacheable))
#define SW_IS_CACHEABLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
    SW_TYPE_CACHEABLE))
#define SW_CACHEABLE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
    SW_TYPE_CACHEABLE, SwCacheableInterface))

typedef struct _SwCacheable SwCacheable;
typedef struct _SwCacheableInterface SwCacheableInterface;
struct _SwCacheableInterface {
  GTypeInterface parent_iface;
  const gchar * (*get_id) (SwCacheable *self);
  gboolean (*is_ready) (SwCacheable *self);
  void (*save_into_cache) (SwCacheable *self, GKeyFile *keys,
                           const gchar *group);
};

GType sw_cacheable_get_type (void);

const gchar *sw_cacheable_get_id (SwCacheable *self);
gboolean sw_cacheable_is_ready (SwCacheable *self);
void sw_cacheable_save_into_cache (SwCacheable *self, GKeyFile *keys);


G_END_DECLS

#endif /* _SW_CACHEABLE */

