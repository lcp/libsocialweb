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

#ifndef _SW_CLIENT_ITEM_VIEW
#define _SW_CLIENT_ITEM_VIEW

#include <glib-object.h>

#include <libsocialweb-client/sw-item.h>

G_BEGIN_DECLS

#define SW_TYPE_CLIENT_ITEM_VIEW sw_client_item_view_get_type()

#define SW_CLIENT_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_CLIENT_ITEM_VIEW, SwClientItemView))

#define SW_CLIENT_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_CLIENT_ITEM_VIEW, SwClientItemViewClass))

#define SW_IS_CLIENT_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_CLIENT_ITEM_VIEW))

#define SW_IS_CLIENT_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_CLIENT_ITEM_VIEW))

#define SW_CLIENT_ITEM_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_CLIENT_ITEM_VIEW, SwClientItemViewClass))

typedef struct {
  GObject parent;
} SwClientItemView;

typedef struct {
  GObjectClass parent_class;
  void (*items_added)(SwClientItemView *item_view, GList *items);
  void (*items_removed)(SwClientItemView *item_view, GList *items);
  void (*items_changed)(SwClientItemView *item_view, GList *item);
} SwClientItemViewClass;

GType sw_client_item_view_get_type (void);

SwClientItemView *_sw_client_item_view_new_for_path (const gchar *item_view_path);
void sw_client_item_view_start (SwClientItemView *item_view);
void sw_client_item_view_refresh (SwClientItemView *item_view);
void sw_client_item_view_stop (SwClientItemView *item_view);
void sw_client_item_view_close (SwClientItemView *item_view);

G_END_DECLS

#endif /* _SW_CLIENT_ITEM_VIEW */

