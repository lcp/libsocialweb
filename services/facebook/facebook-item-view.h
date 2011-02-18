/*
 * libsocialweb Facebook service support
 *
 * Copyright (C) 2010 Novell, Inc.
 * Copyright (C) 2010 Collabora Ltd.
 *
 * Authors: Gary Ching-Pang Lin <glin@novell.com>
 *          Thomas Thurman <thomas.thurman@collabora.co.uk>
 *          Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *          Danielle Madeley <danielle.madeley@collabora.co.uk>
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

#ifndef __FACEBOOK_ITEM_VIEW_H__
#define __FACEBOOK_ITEM_VIEW_H__

#include <glib-object.h>
#include <libsocialweb/sw-item-view.h>

G_BEGIN_DECLS

#define SW_TYPE_FACEBOOK_ITEM_VIEW	(sw_facebook_item_view_get_type ())
#define SW_FACEBOOK_ITEM_VIEW(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_FACEBOOK_ITEM_VIEW, SwFacebookItemView))
#define SW_FACEBOOK_ITEM_VIEW_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), SW_TYPE_FACEBOOK_ITEM_VIEW, SwFacebookItemViewClass))
#define SW_IS_FACEBOOK_ITEM_VIEW(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_FACEBOOK_ITEM_VIEW))
#define SW_IS_FACEBOOK_ITEM_VIEW_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), SW_TYPE_FACEBOOK_ITEM_VIEW))
#define SW_FACEBOOK_ITEM_VIEW_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_FACEBOOK_ITEM_VIEW, SwFacebookItemViewClass))

typedef struct _SwFacebookItemView SwFacebookItemView;
typedef struct _SwFacebookItemViewClass SwFacebookItemViewClass;
typedef struct _SwFacebookItemViewPrivate SwFacebookItemViewPrivate;

struct _SwFacebookItemView
{
  SwItemView parent;
  SwFacebookItemViewPrivate *priv;
};

struct _SwFacebookItemViewClass
{
  SwItemViewClass parent_class;
};

GType sw_facebook_item_view_get_type (void);

G_END_DECLS

#endif
