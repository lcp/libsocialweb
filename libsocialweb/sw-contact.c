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

#include <string.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include "sw-contact.h"
#include "sw-debug.h"

G_DEFINE_TYPE (SwContact, sw_contact, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_CONTACT, SwContactPrivate))

struct _SwContactPrivate {
  /* TODO: fix lifecycle */
  SwService *service;
  /* Contact: hash (key: string) -> (GStrv value)
   */
  GHashTable *hash;
  time_t cached_date;
  time_t mtime;
  gint remaining_fetches;
};

enum
{
  PROP_0,
  PROP_READY,
};

enum
{
  CHANGED_SIGNAL,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

static void
sw_contact_dispose (GObject *object)
{
  SwContact *contact = SW_CONTACT (object);
  SwContactPrivate *priv = contact->priv;

  if (priv->hash) {
    g_hash_table_unref (priv->hash);
    priv->hash = NULL;
  }

  G_OBJECT_CLASS (sw_contact_parent_class)->dispose (object);
}

static void
sw_contact_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  SwContact *contact = SW_CONTACT (object);

  switch (property_id)
  {
    case PROP_READY:
      g_value_set_boolean (value, sw_contact_get_ready (contact));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_contact_set_property (GObject *object,
                      guint property_id,
                      const GValue *value,
                      GParamSpec *pspec)
{
  switch (property_id)
  {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
sw_contact_class_init (SwContactClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwContactPrivate));

  object_class->dispose = sw_contact_dispose;
  object_class->get_property = sw_contact_get_property;
  object_class->set_property = sw_contact_set_property;

  pspec = g_param_spec_boolean ("ready",
                                "ready",
                                "Whether contact is ready to set out",
                                FALSE,
                                G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_READY, pspec);

  signals[CHANGED_SIGNAL] = g_signal_new ("changed",
                                       SW_TYPE_CONTACT,
                                       G_SIGNAL_RUN_FIRST,
                                       G_STRUCT_OFFSET (SwContactClass, changed),
                                       NULL,
                                       NULL,
                                       g_cclosure_marshal_VOID__VOID,
                                       G_TYPE_NONE,
                                       0);
}

static void
sw_contact_init (SwContact *self)
{
  self->priv = GET_PRIVATE (self);
  self->priv->hash = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) g_strfreev);

}

SwContact*
sw_contact_new (void)
{
  return g_object_new (SW_TYPE_CONTACT, NULL);
}

void
sw_contact_set_service (SwContact *contact, SwService *service)
{
  g_return_if_fail (SW_IS_CONTACT (contact));
  g_return_if_fail (SW_IS_SERVICE (service));

  /* TODO: weak reference? Remember to update dispose() */
  contact->priv->service = service;
}

SwService *
sw_contact_get_service (SwContact *contact)
{
  g_return_val_if_fail (SW_IS_CONTACT (contact), NULL);

  return contact->priv->service;
}

void
sw_contact_put (SwContact *contact, const char *key, const char *value)
{
  g_return_if_fail (SW_IS_CONTACT (contact));
  g_return_if_fail (key);

  GStrv str_array;
  GStrv new_str_array;
  str_array = g_hash_table_lookup (contact->priv->hash,
                               (gpointer)g_intern_string (key));
  if (str_array == NULL) {
    new_str_array = g_new0 (gchar *, 2);
    new_str_array[0] = g_strdup (value);
  } else {
    int i;
    int len = g_strv_length (str_array);
    new_str_array = g_new0 (gchar *, len + 2);
    for (i = 0 ; i < len ; i++)
      new_str_array[i] = g_strdup (str_array[i]);
    new_str_array[len] = g_strdup (value);
  }
  g_hash_table_insert (contact->priv->hash,
                       (gpointer)g_intern_string (key),
                       new_str_array);

  sw_contact_touch (contact);
}

void
sw_contact_take (SwContact *contact, const char *key, char *value)
{
  g_return_if_fail (SW_IS_CONTACT (contact));
  g_return_if_fail (key);

  GStrv str_array;
  GStrv new_str_array;
  str_array = g_hash_table_lookup (contact->priv->hash,
                               (gpointer)g_intern_string (key));
  if (str_array == NULL) {
    new_str_array = g_new0 (gchar *, 2);
    new_str_array[0] = value;
  } else {
    int i;
    int len = g_strv_length (str_array);
    new_str_array = g_new0 (gchar *, len + 2);
    for (i = 0 ; i < len ; i++)
      new_str_array[i] = g_strdup (str_array[i]);
    new_str_array[len] = value;
  }
  g_hash_table_insert (contact->priv->hash,
                       (gpointer)g_intern_string (key),
                       new_str_array);

  sw_contact_touch (contact);
}

const char *
sw_contact_get (const SwContact *contact, const char *key)
{
  g_return_val_if_fail (SW_IS_CONTACT (contact), NULL);
  g_return_val_if_fail (key, NULL);

  GStrv str_array = g_hash_table_lookup (contact->priv->hash,
      g_intern_string (key));
  if (!str_array)
    return NULL;
  return str_array[0];
}

static const GStrv
sw_contact_get_all (const SwContact *contact, const char *key)
{
  g_return_val_if_fail (SW_IS_CONTACT (contact), NULL);
  g_return_val_if_fail (key, NULL);

  return g_hash_table_lookup (contact->priv->hash,
        g_intern_string (key));
}

static void
cache_date (SwContact *contact)
{
  const char *s;

  if (contact->priv->cached_date)
    return;

  s = sw_contact_get (contact, "date");
  if (!s)
    return;

  contact->priv->cached_date = sw_time_t_from_string (s);
}

void
sw_contact_dump (SwContact *contact)
{
  GHashTableIter iter;
  const char *key;
  gpointer value;

  g_return_if_fail (SW_IS_CONTACT (contact));

  g_printerr ("SwContact %p\n", contact);
  g_hash_table_iter_init (&iter, contact->priv->hash);
  while (g_hash_table_iter_next (&iter,
                                 (gpointer)&key,
                                 &value)) {
    gchar *concat = g_strjoinv (",", (GStrv) value);
    g_printerr (" %s=%s\n", key, concat);
    g_free (concat);
  }
}

static guint
contact_hash (gconstpointer key)
{
  const SwContact *contact = key;
  return g_str_hash (sw_contact_get (contact, "id"));
}

gboolean
contact_equal (gconstpointer a, gconstpointer b)
{
  const SwContact *contact_a = a;
  const SwContact *contact_b = b;

  return g_str_equal (sw_contact_get (contact_a, "id"),
                      sw_contact_get (contact_b, "id"));
}

SwSet *
sw_contact_set_new (void)
{
  return sw_set_new_full (contact_hash, contact_equal);
}

GHashTable *
sw_contact_peek_hash (SwContact *contact)
{
  g_return_val_if_fail (SW_IS_CONTACT (contact), NULL);

  return contact->priv->hash;
}

gboolean
sw_contact_get_ready (SwContact *contact)
{
  return (contact->priv->remaining_fetches == 0);
}

void
sw_contact_push_pending (SwContact *contact)
{
  g_atomic_int_inc (&(contact->priv->remaining_fetches));
}

void
sw_contact_pop_pending (SwContact *contact)
{
  if (g_atomic_int_dec_and_test (&(contact->priv->remaining_fetches))) {
    SW_DEBUG (CONTACT, "All outstanding fetches completed. Signalling ready: %s",
                  sw_contact_get (contact, "id"));
    g_object_notify (G_OBJECT (contact), "ready");
  }

  sw_contact_touch (contact);
}


typedef struct {
  SwContact *contact;
  const gchar *key;
  gboolean delays_ready;
} RequestImageFetchClosure;

static void
_image_download_cb (const char               *url,
                    char                     *file,
                    RequestImageFetchClosure *closure)
{
  SW_DEBUG (CONTACT, "Image fetched: %s to %s", url, file);
  sw_contact_take (closure->contact,
                    closure->key,
                    file);

  if (closure->delays_ready)
    sw_contact_pop_pending (closure->contact);

  g_object_unref (closure->contact);
  g_slice_free (RequestImageFetchClosure, closure);
}

void
sw_contact_request_image_fetch (SwContact      *contact,
                             gboolean     delays_ready,
                             const gchar *key,
                             const gchar *url)
{
  RequestImageFetchClosure *closure;

  /* If this URL fetch should delay the contact being considered ready, or
   * whether the contact is useful without this key.
   */
  if (delays_ready)
    sw_contact_push_pending (contact);

  closure = g_slice_new0 (RequestImageFetchClosure);

  closure->key = g_intern_string (key);
  closure->contact = g_object_ref (contact);
  closure->delays_ready = delays_ready;

  SW_DEBUG (CONTACT, "Scheduling fetch for %s on: %s",
            url,
            sw_contact_get (closure->contact, "id"));
  sw_web_download_image_async (url,
                               (ImageDownloadCallback)_image_download_cb,
                               closure);
}

/*
 * Construct a GValueArray from a SwContact. We use this to construct the
 * data types that the wonderful dbus-glib needs to emit the signal
 */
GValueArray *
_sw_contact_to_value_array (SwContact *contact)
{
  GValueArray *value_array;
  time_t time;

  time = sw_time_t_from_string (sw_contact_get (contact, "date"));

  value_array = g_value_array_new (4);

  value_array = g_value_array_append (value_array, NULL);
  g_value_init (g_value_array_get_nth (value_array, 0), G_TYPE_STRING);
  g_value_set_string (g_value_array_get_nth (value_array, 0),
                      sw_service_get_name (sw_contact_get_service (contact)));

  value_array = g_value_array_append (value_array, NULL);
  g_value_init (g_value_array_get_nth (value_array, 1), G_TYPE_STRING);
  g_value_set_string (g_value_array_get_nth (value_array, 1),
                      sw_contact_get (contact, "id"));

  value_array = g_value_array_append (value_array, NULL);
  g_value_init (g_value_array_get_nth (value_array, 2), G_TYPE_INT64);
  g_value_set_int64 (g_value_array_get_nth (value_array, 2),
                     time);

  value_array = g_value_array_append (value_array, NULL);
  g_value_init (g_value_array_get_nth (value_array, 3),
                dbus_g_type_get_map ("GHashTable",
                  G_TYPE_STRING,
                  G_TYPE_STRV));

  g_value_set_boxed (g_value_array_get_nth (value_array, 3),
                     sw_contact_peek_hash (contact));

  return value_array;
}

void
sw_contact_touch (SwContact *contact)
{
  contact->priv->mtime = time (NULL);

  g_signal_emit (contact, signals[CHANGED_SIGNAL], 0);
}

time_t
sw_contact_get_mtime (SwContact *contact)
{
  return contact->priv->mtime;
}

/* Intentionally don't compare the mtime */
gboolean
sw_contact_equal (SwContact *a,
               SwContact *b)
{
  SwContactPrivate *priv_a = GET_PRIVATE (a);
  SwContactPrivate *priv_b = GET_PRIVATE (b);
  GHashTable *hash_a = priv_a->hash;
  GHashTable *hash_b = priv_b->hash;
  GHashTableIter iter_a;
  gpointer key_a, value_a;
  guint size_a, size_b;

  if (priv_a->service != priv_b->service)
    return FALSE;

  if (priv_a->remaining_fetches != priv_b->remaining_fetches)
    return FALSE;

  size_a = g_hash_table_size (hash_a);
  size_b = g_hash_table_size (hash_b);

  if (sw_contact_get (a, "cached"))
    size_a--;

  if (sw_contact_get (b, "cached"))
    size_b--;

  if (size_a != size_b)
    return FALSE;

  g_hash_table_iter_init (&iter_a, hash_a);

  while (g_hash_table_iter_next (&iter_a, &key_a, &value_a)) 
  {
    if (g_str_equal (key_a, "cached"))
      continue;

    GStrv value_b;
    int i;
    value_b = sw_contact_get_all (b, key_a);
    if (g_strv_length (value_a) != g_strv_length (value_b))
      return FALSE;

    for (i = 0 ; i < g_strv_length (value_a) ; i++) {
      if (!g_str_equal (((GStrv)value_a)[i], value_b[i]))
        return FALSE;
    }
  }

  return TRUE;
}

