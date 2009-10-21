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
#include <mojito/mojito-web.h>
#include "mojito-item.h"
#include "mojito-debug.h"

G_DEFINE_TYPE (MojitoItem, mojito_item, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_ITEM, MojitoItemPrivate))

struct _MojitoItemPrivate {
  /* TODO: fix lifecycle */
  MojitoService *service;
  GHashTable *hash;
  time_t cached_date;
  gint remaining_fetches;
};

enum
{
  PROP_0,
  PROP_READY
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

  G_OBJECT_CLASS (mojito_item_parent_class)->dispose (object);
}

static void
mojito_item_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  MojitoItem *item = MOJITO_ITEM (object);

  switch (property_id)
  {
    case PROP_READY:
      g_value_set_boolean (value, mojito_item_get_ready (item));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}


static void
mojito_item_class_init (MojitoItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MojitoItemPrivate));

  object_class->dispose = mojito_item_dispose;
  object_class->get_property = mojito_item_get_property;

  pspec = g_param_spec_boolean ("ready",
                                "ready",
                                "Whether item is ready to set out",
                                FALSE,
                                G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_READY, pspec);
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
  item->priv->service = service;
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

  if (value)
    g_hash_table_insert (item->priv->hash, (gpointer)g_intern_string (key), g_strdup (value));
  else
    g_hash_table_remove (item->priv->hash, (gpointer)g_intern_string (key));
}

void
mojito_item_take (MojitoItem *item, const char *key, char *value)
{
  g_return_if_fail (MOJITO_IS_ITEM (item));
  g_return_if_fail (key);

  if (value)
    g_hash_table_insert (item->priv->hash, (gpointer)g_intern_string (key), value);
  else
    g_hash_table_remove (item->priv->hash, (gpointer)g_intern_string (key));
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

gboolean
mojito_item_get_ready (MojitoItem *item)
{
  return (item->priv->remaining_fetches == 0);
}

void
mojito_item_push_pending (MojitoItem *item)
{
  g_atomic_int_inc (&(item->priv->remaining_fetches));
}

void
mojito_item_pop_pending (MojitoItem *item)
{
  if (g_atomic_int_dec_and_test (&(item->priv->remaining_fetches))) {
    MOJITO_DEBUG (ITEM, "All outstanding fetches completed. Signalling ready: %s",
                  mojito_item_get (item, "id"));
    g_object_notify (G_OBJECT (item), "ready");
  }
}


typedef struct {
  MojitoItem *item;
  const gchar *key;
} RequestImageFetchClosure;

static void
_image_download_cb (const char               *url,
                    char                     *file,
                    RequestImageFetchClosure *closure)
{
  MOJITO_DEBUG (ITEM, "Image fetched: %s to %s", url, file);
  mojito_item_take (closure->item,
                    closure->key,
                    file);

  mojito_item_pop_pending (closure->item);

  g_object_unref (closure->item);
  g_slice_free (RequestImageFetchClosure, closure);
}

void
mojito_item_request_image_fetch (MojitoItem  *item,
                                 const gchar *key,
                                 const gchar *url)
{
  RequestImageFetchClosure *closure;

  mojito_item_push_pending (item);

  closure = g_slice_new0 (RequestImageFetchClosure);

  closure->key = g_intern_string (key);
  closure->item = g_object_ref (item);

  MOJITO_DEBUG (ITEM, "Scheduling fetch for %s on: %s",
                url,
                mojito_item_get (closure->item, "id"));
  mojito_web_download_image_async (url,
                                   (ImageDownloadCallback)_image_download_cb,
                                   closure);
}
