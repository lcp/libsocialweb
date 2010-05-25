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

#ifndef _SW_CLIENT_VIEW
#define _SW_CLIENT_VIEW

#include <glib-object.h>

#include <libsocialweb-client/sw-item.h>
#include <libsocialweb-client/sw-client-item-view.h>

G_BEGIN_DECLS

#define SW_TYPE_CLIENT_VIEW SW_TYPE_ITEM_VIEW

#define SW_CLIENT_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_CLIENT_VIEW, SwClientView))

#define SW_CLIENT_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_CLIENT_VIEW, SwClientViewClass))

#define SW_IS_CLIENT_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_CLIENT_VIEW))

#define SW_IS_CLIENT_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_CLIENT_VIEW))

#define SW_CLIENT_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_CLIENT_VIEW, SwClientViewClass))

typedef SwClientItemView SwClientView G_GNUC_DEPRECATED;
typedef SwClientItemViewClass SwClientViewClass G_GNUC_DEPRECATED;

#define sw_client_view_start sw_client_item_view_start
#define sw_client_view_refresh sw_client_item_view_refresh

G_END_DECLS

#endif /* _SW_CLIENT_VIEW */

