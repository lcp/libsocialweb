/*
 * libsocialweb Plurk service support
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

#ifndef _SW_SERVICE_PLURK
#define _SW_SERVICE_PLURK

#include <libsocialweb/sw-service.h>

G_BEGIN_DECLS

#define SW_TYPE_SERVICE_PLURK sw_service_plurk_get_type()

#define SW_SERVICE_PLURK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_SERVICE_PLURK, SwServicePlurk))

#define SW_SERVICE_PLURK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_SERVICE_PLURK, SwServicePlurkClass))

#define SW_IS_SERVICE_PLURK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_SERVICE_PLURK))

#define SW_IS_SERVICE_PLURK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_SERVICE_PLURK))

#define SW_SERVICE_PLURK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_SERVICE_PLURK, SwServicePlurkClass))

typedef struct _SwServicePlurkPrivate SwServicePlurkPrivate;

typedef struct {
  SwService parent;
  SwServicePlurkPrivate *priv;
} SwServicePlurk;

typedef struct {
  SwServiceClass parent_class;
} SwServicePlurkClass;

GType sw_service_plurk_get_type (void);

G_END_DECLS

#endif /* _SW_SERVICE_PLURK */
