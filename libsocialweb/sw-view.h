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

#ifndef _SW_VIEW
#define _SW_VIEW

#include <glib-object.h>
#include <libsocialweb/sw-types.h>
#include <libsocialweb/sw-service.h>

G_BEGIN_DECLS

#define SW_TYPE_VIEW sw_view_get_type()

#define SW_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_VIEW, SwView))

#define SW_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_VIEW, SwViewClass))

#define SW_IS_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_VIEW))

#define SW_IS_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_VIEW))

#define SW_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_VIEW, SwViewClass))

typedef struct _SwViewPrivate SwViewPrivate;

struct _SwView {
  GObject parent;
  SwViewPrivate *priv;
};

typedef struct {
  GObjectClass parent_class;
} SwViewClass;

GType sw_view_get_type (void);

SwView * sw_view_new (SwCore *core, guint count);

void sw_view_add_service (SwView *view, SwService *service, GHashTable *params);

void sw_view_recalculate (SwView *view);

G_END_DECLS

#endif /* _SW_VIEW */
