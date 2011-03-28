/*
 * libsocialweb Facebook service support
 *
 * Copyright (C) 2010 Novell, Inc.
 * Copyright (C) 2010-2011 Collabora Ltd.
 *
 * Authors: Gary Ching-Pang Lin <glin@novell.com>
 *          Thomas Thurman <thomas.thurman@collabora.co.uk>
 *          Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *          Danielle Madeley <danielle.madeley@collabora.co.uk>
 *          Alban Crequy <alban.crequy@collabora.co.uk>
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

#ifndef __FACEBOOK_CONTACT_VIEW_H__
#define __FACEBOOK_CONTACT_VIEW_H__

#include <glib-object.h>
#include <libsocialweb/sw-contact-view.h>

G_BEGIN_DECLS

#define SW_TYPE_FACEBOOK_CONTACT_VIEW	(sw_facebook_contact_view_get_type ())
#define SW_FACEBOOK_CONTACT_VIEW(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_FACEBOOK_CONTACT_VIEW, SwFacebookContactView))
#define SW_FACEBOOK_CONTACT_VIEW_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), SW_TYPE_FACEBOOK_CONTACT_VIEW, SwFacebookContactViewClass))
#define SW_IS_FACEBOOK_CONTACT_VIEW(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_FACEBOOK_CONTACT_VIEW))
#define SW_IS_FACEBOOK_CONTACT_VIEW_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), SW_TYPE_FACEBOOK_CONTACT_VIEW))
#define SW_FACEBOOK_CONTACT_VIEW_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_FACEBOOK_CONTACT_VIEW, SwFacebookContactViewClass))

typedef struct _SwFacebookContactView SwFacebookContactView;
typedef struct _SwFacebookContactViewClass SwFacebookContactViewClass;
typedef struct _SwFacebookContactViewPrivate SwFacebookContactViewPrivate;

struct _SwFacebookContactView
{
  SwContactView parent;
  SwFacebookContactViewPrivate *priv;
};

struct _SwFacebookContactViewClass
{
  SwContactViewClass parent_class;
};

GType sw_facebook_contact_view_get_type (void);

G_END_DECLS

#endif
