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

#ifndef _LIBSOCIALWEB_SERVICE_FLICKR
#define _LIBSOCIALWEB_SERVICE_FLICKR

#include <libsocialweb/sw-service.h>

G_BEGIN_DECLS

#define SW_TYPE_SERVICE_FLICKR sw_service_flickr_get_type()

#define SW_SERVICE_FLICKR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_SERVICE_FLICKR, SwServiceFlickr))

#define SW_SERVICE_FLICKR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_SERVICE_FLICKR, SwServiceFlickrClass))

#define SW_IS_SERVICE_FLICKR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_SERVICE_FLICKR))

#define SW_IS_SERVICE_FLICKR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_SERVICE_FLICKR))

#define SW_SERVICE_FLICKR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_SERVICE_FLICKR, SwServiceFlickrClass))

typedef struct _SwServiceFlickrPrivate SwServiceFlickrPrivate;

typedef struct {
  SwService parent;
  SwServiceFlickrPrivate *priv;
} SwServiceFlickr;

typedef struct {
  SwServiceClass parent_class;
} SwServiceFlickrClass;

GType sw_service_flickr_get_type (void);

G_END_DECLS

#endif /* _LIBSOCIALWEB_SERVICE_FLICKR */
