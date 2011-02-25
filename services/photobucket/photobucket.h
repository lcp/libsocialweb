/*
 * libsocialweb Photobucket service support
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * Authors: Eitan Isaacson <eitan.isaacson@collabora.co.uk>
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

#ifndef _SW_SERVICE_PHOTOBUCKET
#define _SW_SERVICE_PHOTOBUCKET

#include <glib-object.h>
#include <libsocialweb/sw-service.h>

G_BEGIN_DECLS

#define SW_TYPE_SERVICE_PHOTOBUCKET sw_service_photobucket_get_type()

#define SW_SERVICE_PHOTOBUCKET(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_SERVICE_PHOTOBUCKET, SwServicePhotobucket))

#define SW_SERVICE_PHOTOBUCKET_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_SERVICE_PHOTOBUCKET, SwServicePhotobucketClass))

#define SW_IS_SERVICE_PHOTOBUCKET(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_SERVICE_PHOTOBUCKET))

#define SW_IS_SERVICE_PHOTOBUCKET_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_SERVICE_PHOTOBUCKET))

#define SW_SERVICE_PHOTOBUCKET_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_SERVICE_PHOTOBUCKET, SwServicePhotobucketClass))

typedef struct _SwServicePhotobucketPrivate SwServicePhotobucketPrivate;

typedef struct {
  SwService parent;
  SwServicePhotobucketPrivate *priv;
} SwServicePhotobucket;

typedef struct {
  SwServiceClass parent_class;
} SwServicePhotobucketClass;

GType sw_service_photobucket_get_type (void);

G_END_DECLS

#endif /* _SW_SERVICE_PHOTOBUCKET */
