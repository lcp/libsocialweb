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

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "sw-client-contact-view.h"

#include <interfaces/sw-contact-view-bindings.h>
#include <interfaces/sw-marshals.h>

G_DEFINE_TYPE (SwClientContactView, sw_client_contact_view, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_CLIENT_CONTACT_VIEW, SwClientContactViewPrivate))

typedef struct _SwClientContactViewPrivate SwClientContactViewPrivate;

struct _SwClientContactViewPrivate {
    gchar *object_path;
    DBusGConnection *connection;
    DBusGProxy *proxy;
    GHashTable *uuid_to_contacts;
};

enum
{
  PROP_0,
  PROP_OBJECT_PATH
};

enum
{
  CONTACTS_ADDED_SIGNAL,
  CONTACTS_CHANGED_SIGNAL,
  CONTACTS_REMOVED_SIGNAL,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

#define SW_SERVICE_NAME "com.meego.libsocialweb"
#define SW_SERVICE_CONTACT_VIEW_INTERFACE "com.meego.libsocialweb.ContactView"

static void
sw_client_contact_view_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  SwClientContactViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_OBJECT_PATH:
      g_value_set_string (value, priv->object_path);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_client_contact_view_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  SwClientContactViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_OBJECT_PATH:
      priv->object_path = g_value_dup_string (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_client_contact_view_dispose (GObject *object)
{
  SwClientContactViewPrivate *priv = GET_PRIVATE (object);

  if (priv->connection)
  {
    dbus_g_connection_unref (priv->connection);
    priv->connection = NULL;
  }

  if (priv->proxy)
  {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->uuid_to_contacts)
  {
    g_hash_table_unref (priv->uuid_to_contacts);
    priv->uuid_to_contacts = NULL;
  }

  G_OBJECT_CLASS (sw_client_contact_view_parent_class)->dispose (object);
}

static void
sw_client_contact_view_finalize (GObject *object)
{
  SwClientContactViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->object_path);

  G_OBJECT_CLASS (sw_client_contact_view_parent_class)->finalize (object);
}

static void
_sw_contact_update_from_value_array (SwContact      *contact,
                                  GValueArray *varray)
{
  if (contact->service)
  {
    g_free (contact->service);
    contact->service = NULL;
  }

  contact->service = g_value_dup_string (g_value_array_get_nth (varray, 0));

  if (contact->uuid)
  {
    g_free (contact->uuid);
    contact->uuid = NULL;
  }

  contact->uuid = g_value_dup_string (g_value_array_get_nth (varray, 1));

  contact->date.tv_sec = g_value_get_int64 (g_value_array_get_nth (varray, 2));

  if (contact->props)
  {
    g_hash_table_unref (contact->props);
    contact->props = NULL;
  }

  contact->props = g_value_dup_boxed (g_value_array_get_nth (varray, 3));
}

static SwContact *
_sw_contact_from_value_array (GValueArray *varray)
{
  SwContact *contact;

  contact = sw_contact_new ();
  _sw_contact_update_from_value_array (contact, varray);

  return contact;
}

static void
_proxy_contacts_added_cb (DBusGProxy *proxy,
                          GPtrArray  *contacts,
                          gpointer    userdata)
{
  SwClientContactView *view = SW_CLIENT_CONTACT_VIEW (userdata);
  SwClientContactViewPrivate *priv = GET_PRIVATE (view);
  gint i = 0;
  GList *contacts_list = NULL;

  for (i = 0; i < contacts->len; i++)
  {
    GValueArray *varray = (GValueArray *)g_ptr_array_index (contacts, i);
    SwContact *contact;

    /* First reference dropped when list freed */
    contact = _sw_contact_from_value_array (varray);

    g_hash_table_insert (priv->uuid_to_contacts,
                         g_strdup (contact->uuid),
                         sw_contact_ref (contact));

    contacts_list = g_list_append (contacts_list, contact);
  }

  /* If handler wants a ref then it should ref it up */
  g_signal_emit (view, signals[CONTACTS_ADDED_SIGNAL], 0, contacts_list);

  g_list_foreach (contacts_list, (GFunc)sw_contact_unref, NULL);
  g_list_free (contacts_list);
}

static void
_proxy_contacts_changed_cb (DBusGProxy *proxy,
                            GPtrArray  *contacts,
                            gpointer    userdata)
{
  SwClientContactView *view = SW_CLIENT_CONTACT_VIEW (userdata);
  SwClientContactViewPrivate *priv = GET_PRIVATE (view);
  gint i = 0;
  GList *contacts_list = NULL;

  for (i = 0; i < contacts->len; i++)
  {
    GValueArray *varray = (GValueArray *)g_ptr_array_index (contacts, i);
    SwContact *contact;
    const gchar *uid;

    uid = g_value_get_string (g_value_array_get_nth (varray, 1));

    contact = g_hash_table_lookup (priv->uuid_to_contacts,
                                uid);

    if (contact)
    {
      _sw_contact_update_from_value_array (contact, varray);
      contacts_list = g_list_append (contacts_list, sw_contact_ref (contact));
    } else {
      g_critical (G_STRLOC ": Contact changed before added: %s", uid);
    }
  }

  /* If handler wants a ref then it should ref it up */
  g_signal_emit (view, signals[CONTACTS_CHANGED_SIGNAL], 0, contacts_list);

  g_list_foreach (contacts_list, (GFunc)sw_contact_unref, NULL);
  g_list_free (contacts_list);
}

static void
_proxy_contacts_removed_cb (DBusGProxy *proxy,
                            GPtrArray  *contacts,
                            gpointer    userdata)
{
  SwClientContactView *view = SW_CLIENT_CONTACT_VIEW (userdata);
  SwClientContactViewPrivate *priv = GET_PRIVATE (view);
  gint i = 0;
  GList *contacts_list = NULL;

  for (i = 0; i < contacts->len; i++)
  {
    GValueArray *varray = (GValueArray *)g_ptr_array_index (contacts, i);
    const gchar *uid;
    SwContact *contact;

    uid = g_value_get_string (g_value_array_get_nth (varray, 1));

    contact = g_hash_table_lookup (priv->uuid_to_contacts,
                                uid);

    if (contact)
    {
      /* Must ref up because g_hash_table_remove drops ref */
      contacts_list = g_list_append (contacts_list, sw_contact_ref (contact));
      g_hash_table_remove (priv->uuid_to_contacts, uid);
    }
  }

  /* If handler wants a ref then it should ref it up */
  g_signal_emit (view, signals[CONTACTS_REMOVED_SIGNAL], 0, contacts_list);

  g_list_foreach (contacts_list, (GFunc)sw_contact_unref, NULL);
  g_list_free (contacts_list);
}

static GType
_sw_contacts_get_container_type (void)
{
 return dbus_g_type_get_collection ("GPtrArray",
     dbus_g_type_get_struct ("GValueArray",
         G_TYPE_STRING,
         G_TYPE_STRING,
         G_TYPE_INT64,
         dbus_g_type_get_map ("GHashTable",
             G_TYPE_STRING, G_TYPE_STRV),
         G_TYPE_INVALID));
}

static GType
_sw_contacts_removed_get_container_type (void)
{
  return dbus_g_type_get_collection ("GPtrArray",
      dbus_g_type_get_struct ("GValueArray",
          G_TYPE_STRING,
          G_TYPE_STRV,
          G_TYPE_INVALID));
}

static void
sw_client_contact_view_constructed (GObject *object)
{
  SwClientContactViewPrivate *priv = GET_PRIVATE (object);
  GError *error = NULL;
  DBusConnection *conn;

  priv->connection = dbus_g_bus_get (DBUS_BUS_STARTER, &error);

  if (!priv->connection)
  {
    g_critical (G_STRLOC ": Error getting DBUS connection: %s",
                error->message);
    g_clear_error (&error);
    return;
  }

  conn = dbus_g_connection_get_connection (priv->connection);

  priv->proxy = dbus_g_proxy_new_for_name_owner (priv->connection,
                                                 SW_SERVICE_NAME,
                                                 priv->object_path,
                                                 SW_SERVICE_CONTACT_VIEW_INTERFACE,
                                                 &error);

  if (!priv->proxy)
  {
    g_critical (G_STRLOC ": Error setting up proxy for remote object: %s",
                error->message);
    g_clear_error (&error);
    return;
  }

  dbus_g_proxy_add_signal (priv->proxy,
                           "ContactsAdded",
                           _sw_contacts_get_container_type (),
                           NULL);
  dbus_g_proxy_connect_signal (priv->proxy,
                               "ContactsAdded",
                               (GCallback)_proxy_contacts_added_cb,
                               object,
                               NULL);

  dbus_g_proxy_add_signal (priv->proxy,
                           "ContactsChanged",
                           _sw_contacts_get_container_type (),
                           NULL);
  dbus_g_proxy_connect_signal (priv->proxy,
                               "ContactsChanged",
                               (GCallback)_proxy_contacts_changed_cb,
                               object,
                               NULL);

  dbus_g_proxy_add_signal (priv->proxy,
                           "ContactsRemoved",
                           _sw_contacts_removed_get_container_type (),
                           NULL);
  dbus_g_proxy_connect_signal (priv->proxy,
                               "ContactsRemoved",
                               (GCallback)_proxy_contacts_removed_cb,
                               object,
                               NULL);

}

static void
sw_client_contact_view_class_init (SwClientContactViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwClientContactViewPrivate));

  object_class->get_property = sw_client_contact_view_get_property;
  object_class->set_property = sw_client_contact_view_set_property;
  object_class->dispose = sw_client_contact_view_dispose;
  object_class->finalize = sw_client_contact_view_finalize;
  object_class->constructed = sw_client_contact_view_constructed;

  pspec = g_param_spec_string ("object-path",
                               "Object path",
                               "DBUS path to the contact_view's object",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH, pspec);

  /**
   * SwClientContactView::contacts-added:
   * @self:
   * @contacts: (type GLib.List) (element-type Sw.Contact):
   */
  signals[CONTACTS_ADDED_SIGNAL] =
    g_signal_new ("contacts-added",
                  SW_TYPE_CLIENT_CONTACT_VIEW,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (SwClientContactViewClass, contacts_added),
                  NULL,
                  NULL,
                  sw_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);

  /**
   * SwClientContactView::contacts-removed:
   * @self:
   * @contacts: (type GLib.List) (element-type Sw.Contact):
   */
  signals[CONTACTS_REMOVED_SIGNAL] =
    g_signal_new ("contacts-removed",
                  SW_TYPE_CLIENT_CONTACT_VIEW,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (SwClientContactViewClass, contacts_removed),
                  NULL,
                  NULL,
                  sw_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);

  /**
   * SwClientContactView::contacts-changed:
   * @self:
   * @contacts: (type GLib.List) (element-type Sw.Contact):
   */
  signals[CONTACTS_CHANGED_SIGNAL] =
    g_signal_new ("contacts-changed",
                  SW_TYPE_CLIENT_CONTACT_VIEW,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (SwClientContactViewClass, contacts_changed),
                  NULL,
                  NULL,
                  sw_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);

}

static void
sw_client_contact_view_init (SwClientContactView *self)
{
  SwClientContactViewPrivate *priv = GET_PRIVATE (self);


  priv->uuid_to_contacts = g_hash_table_new_full (g_str_hash,
                                               g_str_equal,
                                               g_free,
                                               (GDestroyNotify)sw_contact_unref);
}

SwClientContactView *
_sw_client_contact_view_new_for_path (const gchar *contact_view_path)
{
  return g_object_new (SW_TYPE_CLIENT_CONTACT_VIEW,
                       "object-path", contact_view_path,
                       NULL);
}

/* This is to avoid multiple almost identical callbacks */
static void
_sw_client_contact_view_generic_cb (DBusGProxy *proxy,
                                 GError     *error,
                                 gpointer    userdata)
{
  if (error)
  {
    g_warning (G_STRLOC ": Error when calling %s: %s",
               (const gchar *)userdata,
               error->message);
    g_error_free (error);
  }
}

void
sw_client_contact_view_start (SwClientContactView *contact_view)
{
  SwClientContactViewPrivate *priv = GET_PRIVATE (contact_view);

  com_meego_libsocialweb_ContactView_start_async (priv->proxy,
                                               _sw_client_contact_view_generic_cb,
                                               (gpointer)G_STRFUNC);
}

void
sw_client_contact_view_refresh (SwClientContactView *contact_view)
{
  SwClientContactViewPrivate *priv = GET_PRIVATE (contact_view);

  com_meego_libsocialweb_ContactView_refresh_async (priv->proxy,
                                                 _sw_client_contact_view_generic_cb,
                                                 (gpointer)G_STRFUNC);
}

void
sw_client_contact_view_stop (SwClientContactView *contact_view)
{
  SwClientContactViewPrivate *priv = GET_PRIVATE (contact_view);

  com_meego_libsocialweb_ContactView_stop_async (priv->proxy,
                                              _sw_client_contact_view_generic_cb,
                                              (gpointer)G_STRFUNC);
}

void
sw_client_contact_view_close (SwClientContactView *contact_view)
{
  SwClientContactViewPrivate *priv = GET_PRIVATE (contact_view);

  com_meego_libsocialweb_ContactView_close_async (priv->proxy,
                                              _sw_client_contact_view_generic_cb,
                                              (gpointer)G_STRFUNC);
}
