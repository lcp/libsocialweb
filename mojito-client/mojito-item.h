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

#ifndef _MOJITO_ITEM
#define _MOJITO_ITEM

#define MOJITO_TYPE_ITEM (mojito_item_get_type())

#include <glib-object.h>

typedef struct {
  volatile gint refcount;
  gchar *service;
  gchar *uuid;
  GTimeVal date;
  GHashTable *props;
} MojitoItem;

void mojito_item_unref (MojitoItem *item);
MojitoItem *mojito_item_ref (MojitoItem *item);
void mojito_item_free (MojitoItem *item);
MojitoItem *mojito_item_new (void);

gboolean mojito_item_is_from_cache (MojitoItem *item);

GType mojito_item_get_type (void);

#endif
