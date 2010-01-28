/*
 * libsocialweb - social data store
 * Copyright (C) 2009 Intel Corporation.
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

#ifndef _SW_SERVICE_DIGG
#define _SW_SERVICE_DIGG

#include <glib-object.h>
#include <libsocialweb/sw-service.h>

G_BEGIN_DECLS

#define SW_TYPE_SERVICE_DIGG sw_service_digg_get_type()

#define SW_SERVICE_DIGG(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_SERVICE_DIGG, SwServiceDigg))

#define SW_SERVICE_DIGG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_SERVICE_DIGG, SwServiceDiggClass))

#define SW_IS_SERVICE_DIGG(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_SERVICE_DIGG))

#define SW_IS_SERVICE_DIGG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_SERVICE_DIGG))

#define SW_SERVICE_DIGG_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_SERVICE_DIGG, SwServiceDiggClass))

typedef struct _SwServiceDiggPrivate SwServiceDiggPrivate;

typedef struct {
  SwService parent;
  SwServiceDiggPrivate *priv;
} SwServiceDigg;

typedef struct {
  SwServiceClass parent_class;
} SwServiceDiggClass;

GType sw_service_digg_get_type (void);

G_END_DECLS

#endif /* _SW_SERVICE_DIGG */
