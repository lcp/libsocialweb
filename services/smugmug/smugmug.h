/*
 * libsocialweb SmugMug service support
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

#ifndef _SW_SERVICE_SMUGMUG
#define _SW_SERVICE_SMUGMUG

#include <glib-object.h>
#include <libsocialweb/sw-service.h>

G_BEGIN_DECLS

#define SW_TYPE_SERVICE_SMUGMUG sw_service_smugmug_get_type()

#define SW_SERVICE_SMUGMUG(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_SERVICE_SMUGMUG, SwServiceSmugmug))

#define SW_SERVICE_SMUGMUG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_SERVICE_SMUGMUG, SwServiceSmugmugClass))

#define SW_IS_SERVICE_SMUGMUG(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_SERVICE_SMUGMUG))

#define SW_IS_SERVICE_SMUGMUG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_SERVICE_SMUGMUG))

#define SW_SERVICE_SMUGMUG_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_SERVICE_SMUGMUG, SwServiceSmugmugClass))

typedef struct _SwServiceSmugmugPrivate SwServiceSmugmugPrivate;

typedef struct {
  SwService parent;
  SwServiceSmugmugPrivate *priv;
} SwServiceSmugmug;

typedef struct {
  SwServiceClass parent_class;
} SwServiceSmugmugClass;

GType sw_service_smugmug_get_type (void);

G_END_DECLS

#endif /* _SW_SERVICE_SMUGMUG */
