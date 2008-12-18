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
  g_free (item->source);
  g_free (item->uuid);

  if (item->props)
    g_hash_table_unref (item->props);

  g_slice_free (MojitoItem, item);
}

void
mojito_item_ref (MojitoItem *item)
{
  g_atomic_int_inc (&(item->refcount));
}

void
mojito_item_unref (MojitoItem *item)
{
  if (g_atomic_int_dec_and_test (&(item->refcount)))
  {
    mojito_item_free (item);
  }
}

