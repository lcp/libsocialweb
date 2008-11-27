#include "mojito-item.h"

MojitoItem *
mojito_item_new (void)
{
  MojitoItem *item = g_slice_new0 (MojitoItem);

  /* keys are interned, values are not */
  item->props = g_hash_table_new_full (g_str_hash,
                                       g_direct_equal,
                                       NULL,
                                       g_free);
}

void 
mojito_item_free (MojitoItem *item)
{
  g_hash_table_destroy (item->props);
  g_slice_free (MojitoItem, item);
}

void
mojito_item_set_prop (MojitoItem  *item,
                      const gchar *prop,
                      const gchar *value)
{
  g_hash_table_insert (item->props,
                       (gchar *)g_intern_string (prop),
                       g_strdup (value));
}

