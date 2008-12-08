#include "mojito-set.h"

struct _MojitoSet {
  /* Reference count */
  volatile int ref_count;
  /* Hash of object pointer => pointer to the senitel value */
  GHashTable *hash;
};

static const int sentinel = 0xDECAFBAD;

GType
mojito_set_get_type (void)
{
  static GType type = 0;
  if (G_UNLIKELY (type == 0)) {
    type = g_boxed_type_register_static ("MojitoSet",
                                         (GBoxedCopyFunc)mojito_set_ref,
                                         (GBoxedFreeFunc)mojito_set_unref);
  }
  return type;
}

MojitoSet *
mojito_set_new (void)
{
  MojitoSet *set;

  set = g_slice_new0 (MojitoSet);
  set->ref_count = 1;
  set->hash = g_hash_table_new_full (NULL, NULL, g_object_unref, NULL);

  return set;
}

MojitoSet *
mojito_set_ref (MojitoSet *set)
{
  g_return_val_if_fail (set, NULL);
  g_return_val_if_fail (set->ref_count > 0, NULL);

  g_atomic_int_inc (&set->ref_count);

  return set;
}

void
mojito_set_unref (MojitoSet *set)
{
  g_return_if_fail (set);
  g_return_if_fail (set->ref_count > 0);

  if (g_atomic_int_dec_and_test (&set->ref_count)) {
    g_hash_table_unref (set->hash);
    g_slice_free (MojitoSet, set);
  }
}

void
mojito_set_add (MojitoSet *set, GObject *item)
{
  g_return_if_fail (set);
  g_return_if_fail (G_IS_OBJECT (item));

  /* Ensure this is in sync with add_to_set */
  g_hash_table_insert (set->hash, g_object_ref (item), (gpointer)&sentinel);
}

void
mojito_set_remove (MojitoSet *set, GObject *item)
{
  g_return_if_fail (set);
  g_return_if_fail (G_IS_OBJECT (item));

  g_hash_table_remove (set->hash, item);
}

gboolean
mojito_set_has (MojitoSet *set, GObject *item)
{
  g_return_val_if_fail (set, FALSE);
  g_return_val_if_fail (G_IS_OBJECT (item), FALSE);

  return g_hash_table_lookup (set->hash, item) != NULL;
}

gboolean
mojito_set_is_empty (MojitoSet *set)
{
  g_return_val_if_fail (set, FALSE);

  return g_hash_table_size (set->hash) == 0;
}

void
mojito_set_empty (MojitoSet *set)
{
  g_return_if_fail (set);

  g_hash_table_remove_all (set->hash);
}


static void
add_to_set (gpointer key, gpointer value, gpointer user_data)
{
  GObject *object = key;
  MojitoSet *set = user_data;

  /* Ensure this is in sync with mojito_set_add */
  g_hash_table_insert (set->hash, g_object_ref (object), (gpointer)&sentinel);
}

MojitoSet *
mojito_set_union (MojitoSet *set_a, MojitoSet *set_b)
{
  MojitoSet *set;

  g_return_val_if_fail (set_a, NULL);
  g_return_val_if_fail (set_b, NULL);

  set = mojito_set_new ();

  g_hash_table_foreach (set_a->hash, add_to_set, set);
  g_hash_table_foreach (set_b->hash, add_to_set, set);

  return set;
}
