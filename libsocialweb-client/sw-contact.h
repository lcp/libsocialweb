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

#ifndef _SW_CONTACT
#define _SW_CONTACT

#define SW_TYPE_CONTACT (sw_contact_get_type())

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct {
  volatile gint refcount;
  gchar *service;
  gchar *uuid;
  GTimeVal date;
  GHashTable *props;
} SwContact;

void sw_contact_unref (SwContact *contact);
SwContact *sw_contact_ref (SwContact *contact);
void sw_contact_free (SwContact *contact);
SwContact *sw_contact_new (void);

gboolean sw_contact_is_from_cache (SwContact *contact);
gboolean sw_contact_has_key (SwContact  *contact,
                              const gchar *key);
const gchar *sw_contact_get_value (SwContact  *contact,
                                    const gchar *key);
const GStrv sw_contact_get_value_all (SwContact  *contact,
                                    const gchar *key);

GType sw_contact_get_type (void);

G_END_DECLS
#endif
