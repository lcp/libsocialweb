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

#include "sw-contact.h"

/**
 * SwContact:
 * @refcount:
 * @service:
 * @uuid:
 * @date:
 * @props: (element-type gchar* gchar*):
 */

SwContact *
sw_contact_new (void)
{
  SwContact *contact;

  contact = g_slice_new0 (SwContact);
  contact->refcount = 1;

  return contact;
}

void
sw_contact_free (SwContact *contact)
{
  g_free (contact->service);
  g_free (contact->uuid);

  if (contact->props)
    g_hash_table_unref (contact->props);

  g_slice_free (SwContact, contact);
}

SwContact *
sw_contact_ref (SwContact *contact)
{
  g_atomic_int_inc (&(contact->refcount));
  return contact;
}

void
sw_contact_unref (SwContact *contact)
{
  if (g_atomic_int_dec_and_test (&(contact->refcount)))
  {
    sw_contact_free (contact);
  }
}

GType
sw_contact_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
  {
    type = g_boxed_type_register_static ("SwContact",
                                         (GBoxedCopyFunc)sw_contact_ref,
                                         (GBoxedFreeFunc)sw_contact_unref);
  }

  return type;
}

gboolean
sw_contact_is_from_cache (SwContact *contact)
{
  return g_hash_table_lookup (contact->props, "cached") != NULL;
}

gboolean
sw_contact_has_key (SwContact  *contact,
                     const gchar *key)
{
  return (g_hash_table_lookup (contact->props, key) != NULL);
}

const gchar *
sw_contact_get_value (SwContact  *contact,
                       const gchar *key)
{
  GStrv str_array = g_hash_table_lookup (contact->props, key);
  return str_array ? str_array[0] : NULL;
}

/**
 * sw_contact_get_value_all: 
 *
 * @contact: :
 * @key: :
 *
 * Returns: (transfer none) (array zero-terminated=1) (element-type char*): 
 */
const GStrv
sw_contact_get_value_all (SwContact  *contact,
                       const gchar *key)
{
  return g_hash_table_lookup (contact->props, key);
}
