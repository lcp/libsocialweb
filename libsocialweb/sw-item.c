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

#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include "sw-item.h"
#include "sw-debug.h"

G_DEFINE_TYPE (SwItem, sw_item, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_ITEM, SwItemPrivate))

struct _SwItemPrivate {
  /* TODO: fix lifecycle */
  SwService *service;
  GHashTable *hash;
  time_t cached_date;
  time_t mtime;
  gint remaining_fetches;
};

enum
{
  PROP_0,
  PROP_READY
};

static void
sw_item_dispose (GObject *object)
{
  SwItem *item = SW_ITEM (object);
  SwItemPrivate *priv = item->priv;

  if (priv->hash) {
    g_hash_table_unref (priv->hash);
    priv->hash = NULL;
  }

  G_OBJECT_CLASS (sw_item_parent_class)->dispose (object);
}

static void
sw_item_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  SwItem *item = SW_ITEM (object);

  switch (property_id)
  {
    case PROP_READY:
      g_value_set_boolean (value, sw_item_get_ready (item));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}


static void
sw_item_class_init (SwItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwItemPrivate));

  object_class->dispose = sw_item_dispose;
  object_class->get_property = sw_item_get_property;

  pspec = g_param_spec_boolean ("ready",
                                "ready",
                                "Whether item is ready to set out",
                                FALSE,
                                G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_READY, pspec);
}

static void
sw_item_init (SwItem *self)
{
  self->priv = GET_PRIVATE (self);

  self->priv->hash = g_hash_table_new_full (NULL, NULL, NULL, g_free);
}

SwItem*
sw_item_new (void)
{
  return g_object_new (SW_TYPE_ITEM, NULL);
}

void
sw_item_set_service (SwItem *item, SwService *service)
{
  g_return_if_fail (SW_IS_ITEM (item));
  g_return_if_fail (SW_IS_SERVICE (service));

  /* TODO: weak reference? Remember to update dispose() */
  item->priv->service = service;
}

SwService *
sw_item_get_service (SwItem *item)
{
  g_return_val_if_fail (SW_IS_ITEM (item), NULL);

  return item->priv->service;
}

void
sw_item_put (SwItem *item, const char *key, const char *value)
{
  g_return_if_fail (SW_IS_ITEM (item));
  g_return_if_fail (key);

  if (value)
    g_hash_table_insert (item->priv->hash,
                         (gpointer)g_intern_string (key),
                         g_strdup (value));
  else
    g_hash_table_remove (item->priv->hash,
                         (gpointer)g_intern_string (key));

  sw_item_touch (item);
}

void
sw_item_take (SwItem *item, const char *key, char *value)
{
  g_return_if_fail (SW_IS_ITEM (item));
  g_return_if_fail (key);

  if (value)
    g_hash_table_insert (item->priv->hash,
                         (gpointer)g_intern_string (key),
                         value);
  else
    g_hash_table_remove (item->priv->hash,
                         (gpointer)g_intern_string (key));

  sw_item_touch (item);
}

const char *
sw_item_get (SwItem *item, const char *key)
{
  g_return_val_if_fail (SW_IS_ITEM (item), NULL);
  g_return_val_if_fail (key, NULL);

  return g_hash_table_lookup (item->priv->hash, g_intern_string (key));
}

static void
cache_date (SwItem *item)
{
  const char *s;

  if (item->priv->cached_date)
    return;

  s = g_hash_table_lookup (item->priv->hash, g_intern_string ("date"));
  if (!s)
    return;

  item->priv->cached_date = sw_time_t_from_string (s);
}

int
sw_item_compare_date_older (SwItem *a, SwItem *b)
{
  g_return_val_if_fail (SW_IS_ITEM (a), 0);
  g_return_val_if_fail (SW_IS_ITEM (b), 0);

  cache_date (a);
  cache_date (b);

  return a->priv->cached_date - b->priv->cached_date;
}

int
sw_item_compare_date_newer (SwItem *a, SwItem *b)
{
  g_return_val_if_fail (SW_IS_ITEM (a), 0);
  g_return_val_if_fail (SW_IS_ITEM (b), 0);

  cache_date (a);
  cache_date (b);

  return b->priv->cached_date - a->priv->cached_date;
}

void
sw_item_dump (SwItem *item)
{
  GHashTableIter iter;
  const char *key, *value;

  g_return_if_fail (SW_IS_ITEM (item));

  g_printerr ("SwItem %p\n", item);
  g_hash_table_iter_init (&iter, item->priv->hash);
  while (g_hash_table_iter_next (&iter,
                                 (gpointer)&key,
                                 (gpointer)&value)) {
    g_printerr (" %s=%s\n", key, value);
  }
}

static guint
item_hash (gconstpointer key)
{
  const SwItem *item = key;
  return g_str_hash (g_hash_table_lookup (item->priv->hash,
                                          g_intern_string ("id")));
}

gboolean
item_equal (gconstpointer a, gconstpointer b)
{
  const SwItem *item_a = a;
  const SwItem *item_b = b;

  return g_str_equal (g_hash_table_lookup (item_a->priv->hash,
                                           g_intern_string ("id")),
                      g_hash_table_lookup (item_b->priv->hash,
                                           g_intern_string ("id")));
}

SwSet *
sw_item_set_new (void)
{
  return sw_set_new_full (item_hash, item_equal);
}

GHashTable *
sw_item_peek_hash (SwItem *item)
{
  g_return_val_if_fail (SW_IS_ITEM (item), NULL);

  return item->priv->hash;
}

gboolean
sw_item_get_ready (SwItem *item)
{
  return (item->priv->remaining_fetches == 0);
}

void
sw_item_push_pending (SwItem *item)
{
  g_atomic_int_inc (&(item->priv->remaining_fetches));
}

void
sw_item_pop_pending (SwItem *item)
{
  if (g_atomic_int_dec_and_test (&(item->priv->remaining_fetches))) {
    SW_DEBUG (ITEM, "All outstanding fetches completed. Signalling ready: %s",
                  sw_item_get (item, "id"));
    g_object_notify (G_OBJECT (item), "ready");
  }

  sw_item_touch (item);
}


typedef struct {
  SwItem *item;
  const gchar *key;
  gboolean delays_ready;
} RequestImageFetchClosure;

static void
_image_download_cb (const char               *url,
                    char                     *file,
                    RequestImageFetchClosure *closure)
{
  SW_DEBUG (ITEM, "Image fetched: %s to %s", url, file);
  sw_item_take (closure->item,
                    closure->key,
                    file);

  if (closure->delays_ready)
    sw_item_pop_pending (closure->item);

  g_object_unref (closure->item);
  g_slice_free (RequestImageFetchClosure, closure);
}

void
sw_item_request_image_fetch (SwItem      *item,
                             gboolean     delays_ready,
                             const gchar *key,
                             const gchar *url)
{
  RequestImageFetchClosure *closure;

  /* If this URL fetch should delay the item being considered ready, or
   * whether the item is useful without this key.
   */
  if (delays_ready)
    sw_item_push_pending (item);

  closure = g_slice_new0 (RequestImageFetchClosure);

  closure->key = g_intern_string (key);
  closure->item = g_object_ref (item);
  closure->delays_ready = delays_ready;

  SW_DEBUG (ITEM, "Scheduling fetch for %s on: %s",
            url,
            sw_item_get (closure->item, "id"));
  sw_web_download_image_async (url,
                               (ImageDownloadCallback)_image_download_cb,
                               closure);
}

/*
 * Construct a GValueArray from a SwItem. We use this to construct the
 * data types that the wonderful dbus-glib needs to emit the signal
 */
GValueArray *
_sw_item_to_value_array (SwItem *item)
{
  GValueArray *value_array;
  time_t time;

  time = sw_time_t_from_string (sw_item_get (item, "date"));

  value_array = g_value_array_new (4);

  value_array = g_value_array_append (value_array, NULL);
  g_value_init (g_value_array_get_nth (value_array, 0), G_TYPE_STRING);
  g_value_set_string (g_value_array_get_nth (value_array, 0),
                      sw_service_get_name (sw_item_get_service (item)));

  value_array = g_value_array_append (value_array, NULL);
  g_value_init (g_value_array_get_nth (value_array, 1), G_TYPE_STRING);
  g_value_set_string (g_value_array_get_nth (value_array, 1),
                      sw_item_get (item, "id"));

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
                     sw_item_peek_hash (item));

  return value_array;
}

void
sw_item_touch (SwItem *item)
{
  item->priv->mtime = time (NULL);
}

time_t
sw_item_get_mtime (SwItem *item)
{
  return item->priv->mtime;
}