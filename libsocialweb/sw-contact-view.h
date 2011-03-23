/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 * Copyright (C) 2011 Collabora Ltd.
 *
 * Author: Rob Bradford <rob@linux.intel.com>
 *         Alban Crequy <alban.crequy@collabora.co.uk>
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
#ifndef _SW_CONTACT_VIEW
#define _SW_CONTACT_VIEW

#include <glib-object.h>

#include <libsocialweb/sw-contact.h>

G_BEGIN_DECLS

#define SW_TYPE_CONTACT_VIEW sw_contact_view_get_type()

#define SW_CONTACT_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_CONTACT_VIEW, SwContactView))

#define SW_CONTACT_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_CONTACT_VIEW, SwContactViewClass))

#define SW_IS_CONTACT_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_CONTACT_VIEW))

#define SW_IS_CONTACT_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_CONTACT_VIEW))

#define SW_CONTACT_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_CONTACT_VIEW, SwContactViewClass))

typedef struct {
  GObject parent;
} SwContactView;

typedef struct {
  GObjectClass parent_class;
  void (*start) (SwContactView *contact_view);
  void (*refresh) (SwContactView *contact_view);
  void (*stop) (SwContactView *contact_view);
  void (*close) (SwContactView *contact_view);
} SwContactViewClass;

GType sw_contact_view_get_type (void);

void sw_contact_view_set_from_set (SwContactView *contact_view,
                                SwSet      *set);

const gchar *sw_contact_view_get_object_path (SwContactView *contact_view);
SwService *sw_contact_view_get_service (SwContactView *contact_view);

G_END_DECLS

#endif /* _SW_CONTACT_VIEW */

