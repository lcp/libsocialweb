/*
 * libsocialweb - social data store
 * Copyright (C) 2010 Intel Corporation.
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

#ifndef _SW_SERVICE_VIMEO
#define _SW_SERVICE_VIMEO

#include <glib-object.h>
#include <libsocialweb/sw-service.h>

G_BEGIN_DECLS

#define SW_TYPE_SERVICE_VIMEO sw_service_vimeo_get_type()

#define SW_SERVICE_VIMEO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_SERVICE_VIMEO, SwServiceVimeo))

#define SW_SERVICE_VIMEO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_SERVICE_VIMEO, SwServiceVimeoClass))

#define SW_IS_SERVICE_VIMEO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_SERVICE_VIMEO))

#define SW_IS_SERVICE_VIMEO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_SERVICE_VIMEO))

#define SW_SERVICE_VIMEO_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_SERVICE_VIMEO, SwServiceVimeoClass))

typedef struct _SwServiceVimeoPrivate SwServiceVimeoPrivate;

typedef struct {
  SwService parent;
  SwServiceVimeoPrivate *priv;
} SwServiceVimeo;

typedef struct {
  SwServiceClass parent_class;
} SwServiceVimeoClass;

GType sw_service_vimeo_get_type (void);

G_END_DECLS

#endif /* _SW_SERVICE_VIMEO */
