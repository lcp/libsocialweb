/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 * Copyright (C) 2011 Collabora Ltd.
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

#ifndef _SW_CLIENT_CONTACT_VIEW
#define _SW_CLIENT_CONTACT_VIEW

#include <glib-object.h>

#include <libsocialweb-client/sw-contact.h>

G_BEGIN_DECLS

#define SW_TYPE_CLIENT_CONTACT_VIEW sw_client_contact_view_get_type()

#define SW_CLIENT_CONTACT_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_CLIENT_CONTACT_VIEW, SwClientContactView))

#define SW_CLIENT_CONTACT_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_CLIENT_CONTACT_VIEW, SwClientContactViewClass))

#define SW_IS_CLIENT_CONTACT_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_CLIENT_CONTACT_VIEW))

#define SW_IS_CLIENT_CONTACT_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_CLIENT_CONTACT_VIEW))

#define SW_CLIENT_CONTACT_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_CLIENT_CONTACT_VIEW, SwClientContactViewClass))

typedef struct {
  GObject parent;
} SwClientContactView;

typedef struct {
  GObjectClass parent_class;
  void (*contacts_added)(SwClientContactView *contact_view, GList *contacts);
  void (*contacts_removed)(SwClientContactView *contact_view, GList *contacts);
  void (*contacts_changed)(SwClientContactView *contact_view, GList *contacts);
} SwClientContactViewClass;

GType sw_client_contact_view_get_type (void);

SwClientContactView *_sw_client_contact_view_new_for_path (const gchar *contact_view_path);
void sw_client_contact_view_start (SwClientContactView *contact_view);
void sw_client_contact_view_refresh (SwClientContactView *contact_view);
void sw_client_contact_view_stop (SwClientContactView *contact_view);
void sw_client_contact_view_close (SwClientContactView *contact_view);

G_END_DECLS

#endif /* _SW_CLIENT_CONTACT_VIEW */

