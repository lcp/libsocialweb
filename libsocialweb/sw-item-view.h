/*
 * libsocialweb - social data store
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
#ifndef _SW_ITEM_VIEW
#define _SW_ITEM_VIEW

#include <glib-object.h>

#include <libsocialweb/sw-item.h>

G_BEGIN_DECLS

#define SW_TYPE_ITEM_VIEW sw_item_view_get_type()

#define SW_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_ITEM_VIEW, SwItemView))

#define SW_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_ITEM_VIEW, SwItemViewClass))

#define SW_IS_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_ITEM_VIEW))

#define SW_IS_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_ITEM_VIEW))

#define SW_ITEM_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_ITEM_VIEW, SwItemViewClass))

typedef struct {
  GObject parent;
} SwItemView;

typedef struct {
  GObjectClass parent_class;
  void (*start) (SwItemView *item_view);
  void (*refresh) (SwItemView *item_view);
  void (*stop) (SwItemView *item_view);
  void (*close) (SwItemView *item_view);
} SwItemViewClass;

GType sw_item_view_get_type (void);

void sw_item_view_set_from_set (SwItemView *item_view,
                                SwSet      *set);
void sw_item_view_remove_by_uid (SwItemView  *item_view,
                                 const gchar *uid);

const gchar *sw_item_view_get_object_path (SwItemView *item_view);
SwService *sw_item_view_get_service (SwItemView *item_view);

G_END_DECLS

#endif /* _SW_ITEM_VIEW */

