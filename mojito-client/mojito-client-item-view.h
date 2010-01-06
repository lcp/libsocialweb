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

#ifndef _MOJITO_CLIENT_ITEM_VIEW
#define _MOJITO_CLIENT_ITEM_VIEW

#include <glib-object.h>

#include <mojito-client/mojito-item.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_CLIENT_ITEM_VIEW mojito_client_item_view_get_type()

#define MOJITO_CLIENT_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_CLIENT_ITEM_VIEW, MojitoClientItemView))

#define MOJITO_CLIENT_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_CLIENT_ITEM_VIEW, MojitoClientItemViewClass))

#define MOJITO_IS_CLIENT_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_CLIENT_ITEM_VIEW))

#define MOJITO_IS_CLIENT_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_CLIENT_ITEM_VIEW))

#define MOJITO_CLIENT_ITEM_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_CLIENT_ITEM_VIEW, MojitoClientItemViewClass))

typedef struct {
  GObject parent;
} MojitoClientItemView;

typedef struct {
  GObjectClass parent_class;
  void (*items_added)(MojitoClientItemView *item_view, GList *items);
  void (*items_removed)(MojitoClientItemView *item_view, GList *items);
  void (*items_changed)(MojitoClientItemView *item_view, GList *item);
} MojitoClientItemViewClass;

GType mojito_client_item_view_get_type (void);

MojitoClientItemView *_mojito_client_item_view_new_for_path (const gchar *item_view_path);
void mojito_client_item_view_start (MojitoClientItemView *item_view);
void mojito_client_item_view_refresh (MojitoClientItemView *item_view);

G_END_DECLS

#endif /* _MOJITO_CLIENT_ITEM_VIEW */

