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

#ifndef _SW_SET
#define _SW_SET

#include <glib-object.h>
#include <libsocialweb/sw-types.h>

G_BEGIN_DECLS

#define SW_TYPE_SET sw_set_get_type ()

GType sw_set_get_type (void);

SwSet *sw_set_new (void);

SwSet * sw_set_new_full (GHashFunc hash_func, GEqualFunc equal_func);

SwSet * sw_set_ref (SwSet *set);

void sw_set_unref (SwSet *set);

void sw_set_add (SwSet *set, GObject *item);

void sw_set_remove (SwSet *set, GObject *item);

gboolean sw_set_has (SwSet *set, GObject *item);

gboolean sw_set_is_empty (SwSet *set);

void sw_set_empty (SwSet *set);

SwSet * sw_set_union (SwSet *set_a, SwSet *set_b);

SwSet * sw_set_difference (SwSet *set_a, SwSet *set_b);

void sw_set_add_from (SwSet *set, SwSet *from);
void sw_set_remove_from (SwSet *set, SwSet *from);

GList * sw_set_as_list (SwSet *set);

SwSet * sw_set_from_list (GList *list);

void sw_set_foreach (SwSet *set, GFunc func, gpointer user_data);

typedef gboolean (*SwSetForeachRemoveFunc) (GObject  *object,
                                            gpointer  user_data);
guint sw_set_foreach_remove (SwSet                  *set,
                             SwSetForeachRemoveFunc  func,
                             gpointer                user_data);

int sw_set_size (SwSet *set);

typedef gboolean (*SwSetFilterFunc) (SwSet    *set,
                                     GObject  *object,
                                     gpointer  user_data);
SwSet *sw_set_filter (SwSet           *set,
                      SwSetFilterFunc  func,
                      gpointer         user_data);

G_END_DECLS

#endif /* _SW_SET */
