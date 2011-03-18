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

#ifndef _SW_ITEM
#define _SW_ITEM

#include <glib-object.h>
#include <libsocialweb/sw-types.h>
#include <libsocialweb/sw-service.h>
#include <libsocialweb/sw-set.h>

G_BEGIN_DECLS

#define SW_TYPE_ITEM sw_item_get_type()

#define SW_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_ITEM, SwItem))

#define SW_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_ITEM, SwItemClass))

#define SW_IS_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_ITEM))

#define SW_IS_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_ITEM))

#define SW_ITEM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_ITEM, SwItemClass))

typedef struct _SwItemPrivate SwItemPrivate;

struct _SwItem {
  GObject parent;
  SwItemPrivate *priv;
};

typedef struct {
  GObjectClass parent_class;
  void (*changed)(SwItem *item);
} SwItemClass;

GType sw_item_get_type (void);

SwItem* sw_item_new (void);

void sw_item_set_service (SwItem *item, SwService *service);

SwService * sw_item_get_service (SwItem *item);

void sw_item_put (SwItem     *item,
                  const char *key,
                  const char *value);

void sw_item_take (SwItem     *item,
                   const char *key,
                   char       *value);

void sw_item_request_image_fetch (SwItem      *item,
                                  gboolean     delays_ready,
                                  const gchar *key,
                                  const gchar *url);

const char * sw_item_get (const SwItem *item, const char *key);

int sw_item_compare_date_older (SwItem *a, SwItem *b);

int sw_item_compare_date_newer (SwItem *a, SwItem *b);

void sw_item_dump (SwItem *item);

GHashTable *sw_item_peek_hash (SwItem *item);

gboolean sw_item_get_ready (SwItem *item);

void sw_item_push_pending (SwItem *item);
void sw_item_pop_pending (SwItem *item);

void sw_item_touch (SwItem *item);
time_t sw_item_get_mtime (SwItem *item);


gboolean sw_item_equal (SwItem *a,
                        SwItem *b);

/* Convenience function */
SwSet *sw_item_set_new (void);

/* Useful for emitting the signals */
GValueArray *_sw_item_to_value_array (SwItem *item);

G_END_DECLS

#endif /* _SW_ITEM */
