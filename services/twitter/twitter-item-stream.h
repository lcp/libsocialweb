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
#ifndef _SW_TWITTER_ITEM_STREAM
#define _SW_TWITTER_ITEM_STREAM

#include <glib-object.h>
#include <libsocialweb/sw-item-stream.h>

G_BEGIN_DECLS

#define SW_TYPE_TWITTER_ITEM_STREAM sw_twitter_item_stream_get_type()

#define SW_TWITTER_ITEM_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_TWITTER_ITEM_STREAM, SwTwitterItemStream))

#define SW_TWITTER_ITEM_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_TWITTER_ITEM_STREAM, SwTwitterItemStreamClass))

#define SW_IS_TWITTER_ITEM_STREAM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_TWITTER_ITEM_STREAM))

#define SW_IS_TWITTER_ITEM_STREAM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_TWITTER_ITEM_STREAM))

#define SW_TWITTER_ITEM_STREAM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_TWITTER_ITEM_STREAM, SwTwitterItemStreamClass))

typedef struct {
  SwItemStream parent;
} SwTwitterItemStream;

typedef struct {
  SwItemStreamClass parent_class;
} SwTwitterItemStreamClass;

GType sw_twitter_item_stream_get_type (void);

G_END_DECLS

#endif /* _SW_TWITTER_ITEM_STREAM */
