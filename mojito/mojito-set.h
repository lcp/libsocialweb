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

#ifndef _MOJITO_SET
#define _MOJITO_SET

#include <glib-object.h>
#include <mojito/mojito-types.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_SET mojito_set_get_type ()

GType mojito_set_get_type (void);

MojitoSet *mojito_set_new (void);

MojitoSet * mojito_set_new_full (GHashFunc hash_func, GEqualFunc equal_func);

MojitoSet * mojito_set_ref (MojitoSet *set);

void mojito_set_unref (MojitoSet *set);

void mojito_set_add (MojitoSet *set, GObject *item);

void mojito_set_remove (MojitoSet *set, GObject *item);

gboolean mojito_set_has (MojitoSet *set, GObject *item);

gboolean mojito_set_is_empty (MojitoSet *set);

void mojito_set_empty (MojitoSet *set);

MojitoSet * mojito_set_union (MojitoSet *set_a, MojitoSet *set_b);

MojitoSet * mojito_set_difference (MojitoSet *set_a, MojitoSet *set_b);

void mojito_set_add_from (MojitoSet *set, MojitoSet *from);

GList * mojito_set_as_list (MojitoSet *set);

MojitoSet * mojito_set_from_list (GList *list);

void mojito_set_foreach (MojitoSet *set, GFunc func, gpointer user_data);

int mojito_set_size (MojitoSet *set);

G_END_DECLS

#endif /* _MOJITO_SET */
