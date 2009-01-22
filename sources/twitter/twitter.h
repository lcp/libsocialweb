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

#ifndef _MOJITO_SOURCE_TWITTER
#define _MOJITO_SOURCE_TWITTER

#include <mojito/mojito-source.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_SOURCE_TWITTER mojito_source_twitter_get_type()

#define MOJITO_SOURCE_TWITTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_SOURCE_TWITTER, MojitoSourceTwitter))

#define MOJITO_SOURCE_TWITTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_SOURCE_TWITTER, MojitoSourceTwitterClass))

#define MOJITO_IS_SOURCE_TWITTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_SOURCE_TWITTER))

#define MOJITO_IS_SOURCE_TWITTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_SOURCE_TWITTER))

#define MOJITO_SOURCE_TWITTER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_SOURCE_TWITTER, MojitoSourceTwitterClass))

typedef struct _MojitoSourceTwitterPrivate MojitoSourceTwitterPrivate;

typedef struct {
  MojitoSource parent;
  MojitoSourceTwitterPrivate *priv;
} MojitoSourceTwitter;

typedef struct {
  MojitoSourceClass parent_class;
} MojitoSourceTwitterClass;

GType mojito_source_twitter_get_type (void);

G_END_DECLS

#endif /* _MOJITO_SOURCE_TWITTER */
