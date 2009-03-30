/*
 * Mojito - social data store
 * Copyright (C) 2009 Intel Corporation.
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

#include <config.h>
#include <glib-object.h>
#include "test-runner.h"

#define test_add(unit_name, func) G_STMT_START {                        \
    extern void func (void);                                            \
    g_test_add_func (unit_name, func); } G_STMT_END

struct _DummyObject {
  GObject parent;
};

struct _DummyObjectClass {
  GObjectClass parent_class;
};

G_DEFINE_TYPE (DummyObject, dummy_object, G_TYPE_OBJECT);
static void dummy_object_class_init (DummyObjectClass *class) {}
static void dummy_object_init (DummyObject *dummy) {}
DummyObject *
dummy_object_new (void)
{
  return g_object_new (TYPE_DUMMY_OBJECT, NULL);
}

int
main (int argc, char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  test_add ("/set/is_empty", test_set_is_empty);
  test_add ("/set/foreach_remove", test_set_foreach_remove);

  test_add ("/view/new", test_view_new);
  test_add ("/view/munge-1", test_view_munge_1);
  test_add ("/view/munge-2", test_view_munge_2);
  test_add ("/view/munge-3", test_view_munge_3);
  test_add ("/view/munge-4", test_view_munge_4);

  test_add ("/cache/absolute", test_cache_absolute);
  test_add ("/cache/relative", test_cache_relative);

  return g_test_run ();
}
