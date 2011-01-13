/*
 * libsocialweb Youtube service support
 *
 * Copyright (C) 2010 Novell, Inc.
 *
 * Author: Gary Ching-Pang Lin <glin@novell.com>
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

#ifndef _SW_SERVICE_YOUTUBE
#define _SW_SERVICE_YOUTUBE

#include <libsocialweb/sw-service.h>

G_BEGIN_DECLS

#define SW_TYPE_SERVICE_YOUTUBE sw_service_youtube_get_type()

#define SW_SERVICE_YOUTUBE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_SERVICE_YOUTUBE, SwServiceYoutube))

#define SW_SERVICE_YOUTUBE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_SERVICE_YOUTUBE, SwServiceYoutubeClass))

#define SW_IS_SERVICE_YOUTUBE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_SERVICE_YOUTUBE))

#define SW_IS_SERVICE_YOUTUBE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_SERVICE_YOUTUBE))

#define SW_SERVICE_YOUTUBE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_SERVICE_YOUTUBE, SwServiceYoutubeClass))

typedef struct _SwServiceYoutubePrivate SwServiceYoutubePrivate;

typedef struct {
  SwService parent;
  SwServiceYoutubePrivate *priv;
} SwServiceYoutube;

typedef struct {
  SwServiceClass parent_class;
} SwServiceYoutubeClass;

GType sw_service_youtube_get_type (void);

const char* sw_service_youtube_get_user_auth (SwServiceYoutube *youtube);

G_END_DECLS

#endif /* _SW_SERVICE_YOUTUBE */
