#include <mojito/mojito-utils.h>
#include "mojito-item.h"

G_DEFINE_TYPE (MojitoItem, mojito_item, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_ITEM, MojitoItemPrivate))

struct _MojitoItemPrivate {
  MojitoSource *source;
  GHashTable *hash;
  time_t cached_date;
};

static void
mojito_item_dispose (GObject *object)
{
  /* TODO */
  G_OBJECT_CLASS (mojito_item_parent_class)->dispose (object);
}

static void
mojito_item_finalize (GObject *object)
{
  /* TODO */
  G_OBJECT_CLASS (mojito_item_parent_class)->finalize (object);
}

static void
mojito_item_class_init (MojitoItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoItemPrivate));

  object_class->dispose = mojito_item_dispose;
  object_class->finalize = mojito_item_finalize;
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
mojito_item_set_source (MojitoItem *item, MojitoSource *source)
{
  g_return_if_fail (MOJITO_IS_ITEM (item));
  g_return_if_fail (MOJITO_IS_SOURCE (source));

  item->priv->source = g_object_ref (source);
}

MojitoSource *
mojito_item_get_source (MojitoItem *item)
{
  g_return_val_if_fail (MOJITO_IS_ITEM (item), NULL);

  return item->priv->source;
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

  g_return_if_fail (item);

  g_printerr ("MojitoItem %p\n", item);
  g_hash_table_iter_init (&iter, item->priv->hash);
  while (g_hash_table_iter_next (&iter, (gpointer)&key, (gpointer)&value)) {
    g_printerr (" %s=%s\n", key, value);
  }
}
