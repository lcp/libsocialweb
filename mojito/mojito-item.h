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

#ifndef _MOJITO_ITEM
#define _MOJITO_ITEM

#include <glib-object.h>
#include <mojito/mojito-types.h>
#include <mojito/mojito-service.h>
#include <mojito/mojito-set.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_ITEM mojito_item_get_type()

#define MOJITO_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_ITEM, MojitoItem))

#define MOJITO_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_ITEM, MojitoItemClass))

#define MOJITO_IS_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_ITEM))

#define MOJITO_IS_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_ITEM))

#define MOJITO_ITEM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_ITEM, MojitoItemClass))

typedef struct _MojitoItemPrivate MojitoItemPrivate;

struct _MojitoItem {
  GObject parent;
  MojitoItemPrivate *priv;
};

typedef struct {
  GObjectClass parent_class;
} MojitoItemClass;

GType mojito_item_get_type (void);

MojitoItem* mojito_item_new (void);

void mojito_item_set_service (MojitoItem *item, MojitoService *service);

MojitoService * mojito_item_get_service (MojitoItem *item);

void mojito_item_put (MojitoItem *item, const char *key, const char *value);

void mojito_item_take (MojitoItem *item, const char *key, char *value);

void mojito_item_request_image_fetch (MojitoItem *item, const gchar *key, const gchar *url);

const char * mojito_item_get (MojitoItem *item, const char *key);

int mojito_item_compare_date_older (MojitoItem *a, MojitoItem *b);

int mojito_item_compare_date_newer (MojitoItem *a, MojitoItem *b);

void mojito_item_dump (MojitoItem *item);

GHashTable *mojito_item_peek_hash (MojitoItem *item);

gboolean mojito_item_get_ready (MojitoItem *item);

void mojito_item_push_pending (MojitoItem *item);
void mojito_item_pop_pending (MojitoItem *item);


/* Convenience function */
MojitoSet * mojito_item_set_new (void);

G_END_DECLS

#endif /* _MOJITO_ITEM */
