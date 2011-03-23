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

#include <glib-object.h>
#include <libsocialweb/sw-types.h>
#include <libsocialweb/sw-service.h>
#include <libsocialweb/sw-set.h>

G_BEGIN_DECLS

#define SW_TYPE_CONTACT sw_contact_get_type()

#define SW_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_CONTACT, SwContact))

#define SW_CONTACT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_CONTACT, SwContactClass))

#define SW_IS_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_CONTACT))

#define SW_IS_CONTACT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_CONTACT))

#define SW_CONTACT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_CONTACT, SwContactClass))

typedef struct _SwContactPrivate SwContactPrivate;

struct _SwContact {
  GObject parent;
  SwContactPrivate *priv;
};

typedef struct {
  GObjectClass parent_class;
  void (*changed)(SwContact *contact);
} SwContactClass;

GType sw_contact_get_type (void);

SwContact* sw_contact_new (void);

void sw_contact_set_service (SwContact *contact, SwService *service);

SwService * sw_contact_get_service (SwContact *contact);

void sw_contact_put (SwContact     *contact,
                  const char *key,
                  const char *value);

void sw_contact_take (SwContact     *contact,
                   const char *key,
                   char       *value);

void sw_contact_request_image_fetch (SwContact      *contact,
                                  gboolean     delays_ready,
                                  const gchar *key,
                                  const gchar *url);

const char * sw_contact_get (const SwContact *contact, const char *key);

void sw_contact_dump (SwContact *contact);

GHashTable *sw_contact_peek_hash (SwContact *contact);

gboolean sw_contact_get_ready (SwContact *contact);

void sw_contact_push_pending (SwContact *contact);
void sw_contact_pop_pending (SwContact *contact);

void sw_contact_touch (SwContact *contact);
time_t sw_contact_get_mtime (SwContact *contact);


gboolean sw_contact_equal (SwContact *a,
                        SwContact *b);

/* Convenience function */
SwSet *sw_contact_set_new (void);

/* Useful for emitting the signals */
GValueArray *_sw_contact_to_value_array (SwContact *contact);

G_END_DECLS

#endif /* _SW_CONTACT */
