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

#ifndef _MOJITO_TWITTER_ITEM_VIEW
#define _MOJITO_TWITTER_ITEM_VIEW

#include <glib-object.h>
#include <mojito/mojito-item-view.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_TWITTER_ITEM_VIEW mojito_twitter_item_view_get_type()

#define MOJITO_TWITTER_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_TWITTER_ITEM_VIEW, MojitoTwitterItemView))

#define MOJITO_TWITTER_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_TWITTER_ITEM_VIEW, MojitoTwitterItemViewClass))

#define MOJITO_IS_TWITTER_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_TWITTER_ITEM_VIEW))

#define MOJITO_IS_TWITTER_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_TWITTER_ITEM_VIEW))

#define MOJITO_TWITTER_ITEM_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_TWITTER_ITEM_VIEW, MojitoTwitterItemViewClass))

typedef struct {
  MojitoItemView parent;
} MojitoTwitterItemView;

typedef struct {
  MojitoItemViewClass parent_class;
} MojitoTwitterItemViewClass;

GType mojito_twitter_item_view_get_type (void);

G_END_DECLS

#endif /* _MOJITO_TWITTER_ITEM_VIEW */

