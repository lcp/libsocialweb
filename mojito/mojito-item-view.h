/*
 * Mojito - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 *
 * Author: Rob Bradford <rob@linux.intel.com>
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
#ifndef _MOJITO_ITEM_VIEW
#define _MOJITO_ITEM_VIEW

#include <glib-object.h>

#include <mojito/mojito-item.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_ITEM_VIEW mojito_item_view_get_type()

#define MOJITO_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_ITEM_VIEW, MojitoItemView))

#define MOJITO_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_ITEM_VIEW, MojitoItemViewClass))

#define MOJITO_IS_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_ITEM_VIEW))

#define MOJITO_IS_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_ITEM_VIEW))

#define MOJITO_ITEM_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_ITEM_VIEW, MojitoItemViewClass))

typedef struct {
  GObject parent;
} MojitoItemView;

typedef struct {
  GObjectClass parent_class;
  void (*start) (MojitoItemView *item_view);
  void (*refresh) (MojitoItemView *item_view);
  void (*stop) (MojitoItemView *item_view);
  void (*close) (MojitoItemView *item_view);
} MojitoItemViewClass;

GType mojito_item_view_get_type (void);

void mojito_item_view_add_item (MojitoItemView *item_view,
                                MojitoItem     *item);
void mojito_item_view_update_item (MojitoItemView *item_view,
                                   MojitoItem     *item);
void mojito_item_view_remove_item (MojitoItemView *item_view,
                                   MojitoItem     *item);

void mojito_item_view_add_items (MojitoItemView *item_view,
                                 GList          *items);
void mojito_item_view_update_items (MojitoItemView *item_view,
                                    GList       *items);
void mojito_item_view_remove_items (MojitoItemView *item_view,
                                    GList          *items);

void mojito_item_view_set_from_set (MojitoItemView *item_view,
                                    MojitoSet      *set);

const gchar *mojito_item_view_get_object_path (MojitoItemView *item_view);
MojitoService *mojito_item_view_get_service (MojitoItemView *item_view);

G_END_DECLS

#endif /* _MOJITO_ITEM_VIEW */

