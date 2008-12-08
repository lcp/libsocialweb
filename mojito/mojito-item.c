#include "mojito-item.h"

G_DEFINE_TYPE (MojitoItem, mojito_item, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_ITEM, MojitoItemPrivate))

struct _MojitoItemPrivate {
  GHashTable *hash;
};

static void
mojito_item_dispose (GObject *object)
{
  G_OBJECT_CLASS (mojito_item_parent_class)->dispose (object);
}

static void
mojito_item_finalize (GObject *object)
{
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
