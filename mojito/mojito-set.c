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

#include "mojito-set.h"

struct _MojitoSet {
  /* Reference count */
  volatile int ref_count;
  /* Hash of object pointer => pointer to the senitel value */
  GHashTable *hash;
};

static const int sentinel = 0xDECAFBAD;

static void
add (MojitoSet *set, GObject *object)
{
  g_hash_table_insert (set->hash, g_object_ref (object), (gpointer)&sentinel);
}

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
  return mojito_set_new_full (NULL, NULL);
}

MojitoSet *
mojito_set_new_full (GHashFunc hash_func, GEqualFunc equal_func)
{
  MojitoSet *set;

  set = g_slice_new0 (MojitoSet);
  set->ref_count = 1;
  set->hash = g_hash_table_new_full (hash_func, equal_func, g_object_unref, NULL);

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

  add (set, item);
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

  add (set, object);
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

/* new set with elements in a but not in b */
MojitoSet *
mojito_set_difference (MojitoSet *set_a, MojitoSet *set_b)
{
  MojitoSet *set;
  GHashTableIter iter;
  GObject *object;

  g_return_val_if_fail (set_a, NULL);
  g_return_val_if_fail (set_b, NULL);

  set = mojito_set_new ();

  g_hash_table_iter_init (&iter, set_a->hash);
  while (g_hash_table_iter_next (&iter, (gpointer)&object, NULL)) {
    if (!g_hash_table_lookup (set_b->hash, object)) {
      add (set, object);
    }
  }

  return set;
}

void
mojito_set_add_from (MojitoSet *set, MojitoSet *from)
{
  g_return_if_fail (set);
  g_return_if_fail (from);

  g_hash_table_foreach (from->hash, add_to_set, set);
}

static void
remove_from_set (gpointer key, gpointer value, gpointer user_data)
{
  MojitoSet *set = user_data;

  mojito_set_remove (set, key);
}

void
mojito_set_remove_from (MojitoSet *set, MojitoSet *from)
{
  g_return_if_fail (set);
  g_return_if_fail (from);

  g_hash_table_foreach (from->hash, remove_from_set, set);
}

GList *
mojito_set_as_list (MojitoSet *set)
{
  GList *list;

  g_return_val_if_fail (set, NULL);

  list = g_hash_table_get_keys (set->hash);
  /* The table owns the keys, so reference them for the caller */
  g_list_foreach (list, (GFunc)g_object_ref, NULL);

  return list;
}

MojitoSet *
mojito_set_from_list (GList *list)
{
  MojitoSet *set;

  set = mojito_set_new ();

  for (; list; list = list->next) {
    add (set, list->data);
  }

  return set;
}

void
mojito_set_foreach (MojitoSet *set, GFunc func, gpointer user_data)
{
  GHashTableIter iter;
  gpointer key;

  g_return_if_fail (set);
  g_return_if_fail (func);

  g_hash_table_iter_init (&iter, set->hash);
  while (g_hash_table_iter_next (&iter, &key, NULL)) {
    func (key, user_data);
  }
}

typedef struct {
  MojitoSetForeachRemoveFunc func;
  gpointer user_data;
} RemoveData;

static gboolean
foreach_remove (gpointer key, gpointer value, gpointer user_data)
{
  RemoveData *data = user_data;
  return data->func (key, data->user_data);
}

guint
mojito_set_foreach_remove (MojitoSet *set,
                           MojitoSetForeachRemoveFunc func,
                           gpointer user_data)
{
  RemoveData data;

  g_return_val_if_fail (set, 0);
  g_return_val_if_fail (func, 0);

  data.func = func;
  data.user_data = user_data;

  return g_hash_table_foreach_remove
    (set->hash, foreach_remove, &data);
}


int
mojito_set_size (MojitoSet *set)
{
  g_return_val_if_fail (set, 0);

  return g_hash_table_size (set->hash);
}

#if BUILD_TESTS

#include "test-runner.h"

void
test_set_is_empty (void)
{
  MojitoSet *set;
  DummyObject *obj;

  set = mojito_set_new ();
  obj = dummy_object_new ();

  g_assert (mojito_set_is_empty (set));

  mojito_set_add (set, G_OBJECT (obj));

  g_assert (!mojito_set_is_empty (set));

  mojito_set_remove (set, G_OBJECT (obj));

  g_assert (mojito_set_is_empty (set));

  g_object_unref (obj);
  mojito_set_unref (set);
}

static gboolean
remove_this (GObject *object, gpointer user_data)
{
  return object == user_data;
}

void
test_set_foreach_remove (void)
{
  MojitoSet *set;
  DummyObject *obj1, *obj2;
  guint count;

  set = mojito_set_new ();
  obj1 = dummy_object_new ();
  obj2 = dummy_object_new ();

  mojito_set_add (set, G_OBJECT (obj1));
  mojito_set_add (set, G_OBJECT (obj2));

  count = mojito_set_foreach_remove (set, remove_this, NULL);
  g_assert_cmpint (count, ==, 0);
  g_assert_cmpint (mojito_set_size (set), ==, 2);

  count = mojito_set_foreach_remove (set, remove_this, obj1);
  g_assert_cmpint (count, ==, 1);
  g_assert_cmpint (mojito_set_size (set), ==, 1);

  count = mojito_set_foreach_remove (set, remove_this, obj1);
  g_assert_cmpint (count, ==, 0);
  g_assert_cmpint (mojito_set_size (set), ==, 1);

  count = mojito_set_foreach_remove (set, remove_this, obj2);
  g_assert_cmpint (count, ==, 1);
  g_assert_cmpint (mojito_set_size (set), ==, 0);

  g_object_unref (obj1);
  g_object_unref (obj2);
  mojito_set_unref (set);
}

#endif
