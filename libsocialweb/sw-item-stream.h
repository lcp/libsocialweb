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
#ifndef _SW_ITEM_STREAM
#define _SW_ITEM_STREAM

#include <glib-object.h>

#include <libsocialweb/sw-item.h>

G_BEGIN_DECLS

#define SW_TYPE_ITEM_STREAM sw_item_stream_get_type()

#define SW_ITEM_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_ITEM_STREAM, SwItemStream))

#define SW_ITEM_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_ITEM_STREAM, SwItemStreamClass))

#define SW_IS_ITEM_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_ITEM_STREAM))

#define SW_IS_ITEM_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_ITEM_STREAM))

#define SW_ITEM_STREAM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_ITEM_STREAM, SwItemStreamClass))

typedef struct {
  GObject parent;
} SwItemStream;

typedef struct {
  GObjectClass parent_class;
  void (*start) (SwItemStream *item_stream);
  void (*refresh) (SwItemStream *item_stream);
  void (*stop) (SwItemStream *item_stream);
  void (*close) (SwItemStream *item_stream);
} SwItemStreamClass;

GType sw_item_stream_get_type (void);

const gchar *sw_item_stream_get_object_path (SwItemStream *item_stream);
SwService *sw_item_stream_get_service (SwItemStream *item_stream);


void sw_item_stream_add_items (SwItemStream *item_stream,
                               GList        *items);
void sw_item_stream_add_item (SwItemStream *item_stream,
                              SwItem       *item);

void sw_item_stream_update_items (SwItemStream *item_stream,
                                  GList        *items);
void sw_item_stream_update_item (SwItemStream *item_stream,
                                 SwItem       *item);

void sw_item_stream_remove_items (SwItemStream *item_stream,
                                  GList        *items);
void sw_item_stream_remove_item (SwItemStream *item_stream,
                                 SwItem       *item);

G_END_DECLS

#endif /* _SW_ITEM_STREAM */

