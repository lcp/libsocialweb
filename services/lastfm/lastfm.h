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

#ifndef _SW_SERVICE_LASTFM
#define _SW_SERVICE_LASTFM

#include <glib-object.h>
#include <libsocialweb/sw-service.h>

G_BEGIN_DECLS

#define SW_TYPE_SERVICE_LASTFM sw_service_lastfm_get_type()

#define SW_SERVICE_LASTFM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_SERVICE_LASTFM, SwServiceLastfm))

#define SW_SERVICE_LASTFM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_SERVICE_LASTFM, SwServiceLastfmClass))

#define SW_IS_SERVICE_LASTFM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_SERVICE_LASTFM))

#define SW_IS_SERVICE_LASTFM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_SERVICE_LASTFM))

#define SW_SERVICE_LASTFM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_SERVICE_LASTFM, SwServiceLastfmClass))

typedef struct _SwServiceLastfmPrivate SwServiceLastfmPrivate;

typedef struct {
  SwService parent;
  SwServiceLastfmPrivate *priv;
} SwServiceLastfm;

typedef struct {
  SwServiceClass parent_class;
} SwServiceLastfmClass;

GType sw_service_lastfm_get_type (void);

const gchar *sw_service_lastfm_get_user_id (SwServiceLastfm *service);

G_END_DECLS

#endif /* _SW_SERVICE_LASTFM */
