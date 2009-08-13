/*
 * Brixton, a Mojito demo
 * Copyright (C) 2009, Intel Corporation.
 *
 * Authors: Rob Bradford <rob@linux.intel.com>
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
 *
 */

#ifndef _BRX_ITEM
#define _BRX_ITEM

#include <glib-object.h>
#include <nbtk/nbtk.h>

G_BEGIN_DECLS

#define BRX_TYPE_ITEM brx_item_get_type()

#define BRX_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRX_TYPE_ITEM, BrxItem))

#define BRX_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), BRX_TYPE_ITEM, BrxItemClass))

#define BRX_IS_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRX_TYPE_ITEM))

#define BRX_IS_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRX_TYPE_ITEM))

#define BRX_ITEM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), BRX_TYPE_ITEM, BrxItemClass))

typedef struct {
  NbtkButton parent;
} BrxItem;

typedef struct {
  NbtkButtonClass parent_class;
} BrxItemClass;

GType brx_item_get_type (void);

G_END_DECLS

#endif /* _BRX_ITEM */


