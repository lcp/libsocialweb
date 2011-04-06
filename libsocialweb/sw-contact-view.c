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
#include "sw-debug.h"
#include "sw-contact-view.h"
#include "sw-contact-view-ginterface.h"

#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-core.h>

static void sw_contact_view_iface_init (gpointer g_iface, gpointer iface_data);
G_DEFINE_TYPE_WITH_CODE (SwContactView, sw_contact_view, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SW_TYPE_CONTACT_VIEW_IFACE,
                                                sw_contact_view_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_CONTACT_VIEW, SwContactViewPrivate))

typedef struct _SwContactViewPrivate SwContactViewPrivate;

struct _SwContactViewPrivate {
  SwService *service;
  gchar *object_path;
  SwSet *current_contacts_set;
  SwSet *pending_contacts_set;

  /* timeout used for coalescing multiple delayed ready additions */
  guint pending_timeout_id;

  /* timeout used for ratelimiting checking for changed contacts */
  guint refresh_timeout_id;

  GHashTable *uid_to_contacts;

  GList *changed_contacts;
};

enum
{
  PROP_0,
  PROP_SERVICE,
  PROP_OBJECT_PATH
};

static void sw_contact_view_add_contacts (SwContactView *contact_view,
                                    GList      *contacts);
static void sw_contact_view_update_contacts (SwContactView *contact_view,
                                       GList      *contacts);
static void sw_contact_view_remove_contacts (SwContactView *contact_view,
                                       GList      *contacts);

static void
sw_contact_view_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  SwContactViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_SERVICE:
      g_value_set_object (value, priv->service);
      break;
    case PROP_OBJECT_PATH:
      g_value_set_string (value, priv->object_path);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_contact_view_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  SwContactViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_SERVICE:
      priv->service = g_value_dup_object (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_contact_view_dispose (GObject *object)
{
  SwContactViewPrivate *priv = GET_PRIVATE (object);

  if (priv->service)
  {
    g_object_unref (priv->service);
    priv->service = NULL;
  }

  if (priv->current_contacts_set)
  {
    sw_set_unref (priv->current_contacts_set);
    priv->current_contacts_set = NULL;
  }

  if (priv->pending_contacts_set)
  {
    sw_set_unref (priv->pending_contacts_set);
    priv->pending_contacts_set = NULL;
  }

  if (priv->uid_to_contacts)
  {
    g_hash_table_unref (priv->uid_to_contacts);
    priv->uid_to_contacts = NULL;
  }

  if (priv->pending_timeout_id)
  {
    g_source_remove (priv->pending_timeout_id);
    priv->pending_timeout_id = 0;
  }

  if (priv->refresh_timeout_id)
  {
    g_source_remove (priv->refresh_timeout_id);
    priv->refresh_timeout_id = 0;
  }

  G_OBJECT_CLASS (sw_contact_view_parent_class)->dispose (object);
}

static void
sw_contact_view_finalize (GObject *object)
{
  SwContactViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->object_path);

  G_OBJECT_CLASS (sw_contact_view_parent_class)->finalize (object);
}

static gchar *
_make_object_path (SwContactView *contact_view)
{
  gchar *path;
  static gint count = 0;

  path = g_strdup_printf ("/com/meego/libsocialweb/View%d",
                          count);

  count++;

  return path;
}

static void
sw_contact_view_constructed (GObject *object)
{
  SwContactView *contact_view = SW_CONTACT_VIEW (object);
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);
  SwCore *core;

  core = sw_core_dup_singleton ();

  priv->object_path = _make_object_path (contact_view);
  dbus_g_connection_register_g_object (sw_core_get_connection (core),
                                       priv->object_path,
                                       G_OBJECT (contact_view));
  g_object_unref (core);
  /* The only reference should be the one on the bus */

  if (G_OBJECT_CLASS (sw_contact_view_parent_class)->constructed)
    G_OBJECT_CLASS (sw_contact_view_parent_class)->constructed (object);
}

/* Default implementation for close */
static void
sw_contact_view_default_close (SwContactView *contact_view)
{
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);
  SwCore *core;

  SW_DEBUG (VIEWS, "%s called on %s", G_STRFUNC, priv->object_path);

  core = sw_core_dup_singleton ();
  dbus_g_connection_unregister_g_object (sw_core_get_connection (core),
                                         G_OBJECT (contact_view));
  g_object_unref (core);

  /* Object is no longer needed */
  g_object_unref (contact_view);
}

static void
sw_contact_view_class_init (SwContactViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwContactViewPrivate));

  object_class->get_property = sw_contact_view_get_property;
  object_class->set_property = sw_contact_view_set_property;
  object_class->dispose = sw_contact_view_dispose;
  object_class->finalize = sw_contact_view_finalize;
  object_class->constructed = sw_contact_view_constructed;

  klass->close = sw_contact_view_default_close;

  pspec = g_param_spec_object ("service",
                               "service",
                               "The service this view is using",
                               SW_TYPE_SERVICE,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_SERVICE, pspec);

  pspec = g_param_spec_string ("object-path",
                               "Object path",
                               "The object path of this view",
                               NULL,
                               G_PARAM_READABLE);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH, pspec);
}

static void
sw_contact_view_init (SwContactView *self)
{
  SwContactViewPrivate *priv = GET_PRIVATE (self);

  priv->current_contacts_set = sw_contact_set_new ();
  priv->pending_contacts_set = sw_contact_set_new ();

  priv->uid_to_contacts = g_hash_table_new_full (g_str_hash,
                                              g_str_equal,
                                              g_free,
                                              g_object_unref);
}

/* DBUS interface to class vfunc bindings */

static void
sw_contact_view_start (SwContactViewIface       *iface,
                    DBusGMethodInvocation *context)
{
  SwContactView *contact_view = SW_CONTACT_VIEW (iface);
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);

  SW_DEBUG (VIEWS, "%s called on %s", G_STRFUNC, priv->object_path);

  if (SW_CONTACT_VIEW_GET_CLASS (iface)->start)
    SW_CONTACT_VIEW_GET_CLASS (iface)->start (contact_view);

  sw_contact_view_iface_return_from_start (context);
}

static void
sw_contact_view_refresh (SwContactViewIface       *iface,
                      DBusGMethodInvocation *context)
{
  SwContactView *contact_view = SW_CONTACT_VIEW (iface);
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);

  SW_DEBUG (VIEWS, "%s called on %s", G_STRFUNC, priv->object_path);

  if (SW_CONTACT_VIEW_GET_CLASS (iface)->refresh)
    SW_CONTACT_VIEW_GET_CLASS (iface)->refresh (contact_view);

  sw_contact_view_iface_return_from_refresh (context);
}

static void
sw_contact_view_stop (SwContactViewIface       *iface,
                   DBusGMethodInvocation *context)
{
  SwContactView *contact_view = SW_CONTACT_VIEW (iface);
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);

  SW_DEBUG (VIEWS, "%s called on %s", G_STRFUNC, priv->object_path);

  if (SW_CONTACT_VIEW_GET_CLASS (iface)->stop)
    SW_CONTACT_VIEW_GET_CLASS (iface)->stop (contact_view);

  sw_contact_view_iface_return_from_stop (context);
}

static void
sw_contact_view_close (SwContactViewIface       *iface,
                    DBusGMethodInvocation *context)
{
  SwContactView *contact_view = SW_CONTACT_VIEW (iface);
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);

  SW_DEBUG (VIEWS, "%s called on %s", G_STRFUNC, priv->object_path);

  if (SW_CONTACT_VIEW_GET_CLASS (iface)->close)
    SW_CONTACT_VIEW_GET_CLASS (iface)->close (contact_view);

  sw_contact_view_iface_return_from_close (context);
}

static void
sw_contact_view_iface_init (gpointer g_iface,
                         gpointer iface_data)
{
  SwContactViewIfaceClass *klass = (SwContactViewIfaceClass*)g_iface;
  sw_contact_view_iface_implement_start (klass, sw_contact_view_start);
  sw_contact_view_iface_implement_refresh (klass, sw_contact_view_refresh);
  sw_contact_view_iface_implement_stop (klass, sw_contact_view_stop);
  sw_contact_view_iface_implement_close (klass, sw_contact_view_close);
}

static gboolean
_handle_ready_pending_cb (gpointer data)
{
  SwContactView *contact_view = SW_CONTACT_VIEW (data);
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);
  GList *contacts_to_send = NULL;
  GList *pending_contacts, *l;

  SW_DEBUG (VIEWS, "Delayed ready timeout fired");

  /* FIXME: Reword this to avoid unnecessary list creation ? */
  pending_contacts = sw_set_as_list (priv->pending_contacts_set);

  for (l = pending_contacts; l; l = l->next)
  {
    SwContact *contact = SW_CONTACT (l->data);

    if (sw_contact_get_ready (contact))
    {
      contacts_to_send = g_list_prepend (contacts_to_send, contact);
      sw_set_remove (priv->pending_contacts_set, (GObject *)contact);
    }
  }

  sw_contact_view_add_contacts (contact_view, contacts_to_send);

  g_list_free (pending_contacts);

  priv->pending_timeout_id = 0;

  return FALSE;
}

static void
_contact_ready_weak_notify_cb (gpointer  data,
                            GObject  *dead_object);

static void
_contact_ready_notify_cb (SwContact     *contact,
                       GParamSpec *pspec,
                       SwContactView *contact_view)
{
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);

  if (sw_contact_get_ready (contact)) {
    SW_DEBUG (VIEWS, "Contact became ready: %s.",
              sw_contact_get (contact, "id"));
    g_signal_handlers_disconnect_by_func (contact,
                                          _contact_ready_notify_cb,
                                          contact_view);
    g_object_weak_unref ((GObject *)contact_view,
                         _contact_ready_weak_notify_cb,
                         contact);

    if (!priv->pending_timeout_id)
    {
      SW_DEBUG (VIEWS, "Setting up timeout");
      priv->pending_timeout_id = g_timeout_add_seconds (1,
                                                        _handle_ready_pending_cb,
                                                        contact_view);
    } else {
      SW_DEBUG (VIEWS, "Timeout already set up.");
    }
  }
}

static void
_contact_ready_weak_notify_cb (gpointer  data,
                            GObject  *dead_object)
{
  g_signal_handlers_disconnect_by_func (data,
                                        _contact_ready_notify_cb,
                                        dead_object);
}

static void
_setup_ready_handler (SwContact     *contact,
                      SwContactView *contact_view)
{
  g_signal_connect (contact,
                    "notify::ready",
                    (GCallback)_contact_ready_notify_cb,
                    contact_view);
  g_object_weak_ref ((GObject *)contact_view,
                     _contact_ready_weak_notify_cb,
                     contact);
}

static gboolean
_contact_changed_timeout_cb (gpointer data)
{
  SwContactView *contact_view = SW_CONTACT_VIEW (data);
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);

  sw_contact_view_update_contacts (contact_view, priv->changed_contacts);
  g_list_foreach (priv->changed_contacts, (GFunc)g_object_unref, NULL);
  g_list_free (priv->changed_contacts);
  priv->changed_contacts = NULL;

  priv->refresh_timeout_id = 0;

  return FALSE;
}

static void
_contact_changed_cb (SwContact     *contact,
                  SwContactView *contact_view)
{
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);

  /* We only care if the contact is ready. If it's not then we don't want to be
   * emitting changed but instead it will be added through the readiness
   * tracking.
   */
  if (!sw_contact_get_ready (contact))
    return;

  if (!g_list_find (priv->changed_contacts, contact))
    priv->changed_contacts = g_list_append (priv->changed_contacts, contact);

  if (!priv->refresh_timeout_id)
  {
    SW_DEBUG (VIEWS, "Contact changed, Setting up timeout");

    priv->refresh_timeout_id = g_timeout_add_seconds (10,
                                                      _contact_changed_timeout_cb,
                                                      contact_view);
  }
}

static void
_contact_changed_weak_notify_cb (gpointer  data,
                              GObject  *dead_object)
{
  SwContact *contact = (SwContact *)data;

  g_signal_handlers_disconnect_by_func (contact,
                                        _contact_changed_cb,
                                        dead_object);
  g_object_unref (contact);
}

static void
_setup_changed_handler (SwContact     *contact,
                        SwContactView *contact_view)
{
  g_signal_connect (contact,
                    "changed",
                    (GCallback)_contact_changed_cb,
                    contact_view);
  g_object_weak_ref ((GObject *)contact_view,
                     _contact_changed_weak_notify_cb,
                     g_object_ref (contact));
}

/**
 * sw_contact_view_add_contacts
 * @contact_view: A #SwContactView
 * @contacts: A list of #SwContact objects
 *
 * Add the contacts supplied in the list from the #SwContactView. In many
 * cases what you actually want is sw_contact_view_remove_from_set() or
 * sw_contact_view_set_from_set(). This will cause signal emissions over the
 * bus.
 *
 * This is used in the implementation of sw_contact_view_remove_from_set()
 */
static void
sw_contact_view_add_contacts (SwContactView *contact_view,
                        GList      *contacts)
{
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);
  GValueArray *value_array;
  GPtrArray *contacts_ptr_array;
  GList *l;

  contacts_ptr_array = g_ptr_array_new_with_free_func
      ((GDestroyNotify)g_value_array_free);

  for (l = contacts; l; l = l->next)
  {
    SwContact *contact = SW_CONTACT (l->data);

    if (sw_contact_get_ready (contact))
    {
      SW_DEBUG (VIEWS, "Contact ready: %s",
                sw_contact_get (contact, "id"));
      value_array = _sw_contact_to_value_array (contact);
      g_ptr_array_add (contacts_ptr_array, value_array);
    } else {
      SW_DEBUG (VIEWS, "Contact not ready, setting up handler: %s",
                sw_contact_get (contact, "id"));
      _setup_ready_handler (contact, contact_view);
      sw_set_add (priv->pending_contacts_set, (GObject *)contact);
    }

    _setup_changed_handler (contact, contact_view);
  }

  SW_DEBUG (VIEWS, "Number of contacts to be added: %d", contacts_ptr_array->len);

  if (contacts_ptr_array->len > 0)
    sw_contact_view_iface_emit_contacts_added (contact_view,
                                            contacts_ptr_array);

  g_ptr_array_free (contacts_ptr_array, TRUE);
}

/**
 * sw_contact_view_update_contacts
 * @contact_view: A #SwContactView
 * @contacts: A list of #SwContact objects that need updating
 *
 * Update the contacts supplied in the list in the #SwContactView. This is
 * will cause signal emissions over the bus.
 */
static void
sw_contact_view_update_contacts (SwContactView *contact_view,
                           GList      *contacts)
{
  GValueArray *value_array;
  GPtrArray *contacts_ptr_array;
  GList *l;

  contacts_ptr_array = g_ptr_array_new_with_free_func
      ((GDestroyNotify)g_value_array_free);

  for (l = contacts; l; l = l->next)
  {
    SwContact *contact = SW_CONTACT (l->data);

    /*
     * Contact must be ready and also not in the pending contacts set; we need to
     * check this to prevent ContactsChanged coming before ContactsAdded
     */
    if (sw_contact_get_ready (contact))
    {
      value_array = _sw_contact_to_value_array (contact);
      g_ptr_array_add (contacts_ptr_array, value_array);
    }
  }

  SW_DEBUG (VIEWS, "Number of contacts to be changed: %d",
      contacts_ptr_array->len);

  if (contacts_ptr_array->len > 0)
    sw_contact_view_iface_emit_contacts_changed (contact_view,
                                           contacts_ptr_array);

  g_ptr_array_free (contacts_ptr_array, TRUE);
}

/**
 * sw_contact_view_remove_contacts
 * @contact_view: A #SwContactView
 * @contacts: A list of #SwContact objects
 *
 * Remove the contacts supplied in the list from the #SwContactView. In many
 * cases what you actually want is sw_contact_view_remove_from_set() or
 * sw_contact_view_set_from_set(). This will cause signal emissions over the
 * bus.
 *
 * This is used in the implementation of sw_contact_view_remove_from_set()
 *
 */
static void
sw_contact_view_remove_contacts (SwContactView *contact_view,
                           GList      *contacts)
{
  GValueArray *value_array;
  GPtrArray *contacts_ptr_array;
  GList *l;
  SwContact *contact;

  contacts_ptr_array = g_ptr_array_new_with_free_func
      ((GDestroyNotify)g_value_array_free);

  for (l = contacts; l; l = l->next)
  {
    contact = SW_CONTACT (l->data);

    value_array = g_value_array_new (2);

    value_array = g_value_array_append (value_array, NULL);
    g_value_init (g_value_array_get_nth (value_array, 0), G_TYPE_STRING);
    g_value_set_string (g_value_array_get_nth (value_array, 0),
                        sw_service_get_name (sw_contact_get_service (contact)));

    value_array = g_value_array_append (value_array, NULL);
    g_value_init (g_value_array_get_nth (value_array, 1), G_TYPE_STRING);
    g_value_set_string (g_value_array_get_nth (value_array, 1),
                        sw_contact_get (contact, "id"));

    g_ptr_array_add (contacts_ptr_array, value_array);
  }

  if (contacts_ptr_array->len > 0)
    sw_contact_view_iface_emit_contacts_removed (contact_view,
                                           contacts_ptr_array);

  g_ptr_array_free (contacts_ptr_array, TRUE);
}

/**
 * sw_contact_view_get_object_path
 * @contact_view: A #SwContactView
 *
 * Since #SwContactView is responsible for constructing the object path and
 * registering the object on the bus. This function is necessary for
 * #SwCore to be able to return the object path as the result of a
 * function to open a view.
 *
 * Returns: A string providing the object path.
 */
const gchar *
sw_contact_view_get_object_path (SwContactView *contact_view)
{
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);

  return priv->object_path;
}

/**
 * sw_contact_view_get_service
 * @contact_view: A #SwContactView
 *
 * Returns: The #SwService that #SwContactView is for
 */
SwService *
sw_contact_view_get_service (SwContactView *contact_view)
{
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);

  return priv->service;
}

/* TODO: Export this function ? */
/**
 * sw_contact_view_add_from_set
 * @contact_view: A #SwContactView
 * @set: A #SwSet
 *
 * Add the contacts that are in the supplied set to the view.
 *
 * This is used in the implementation of sw_contact_view_set_from_set()
 */
void
sw_contact_view_add_from_set (SwContactView *contact_view,
                           SwSet      *set)
{
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);
  GList *contacts;
  GList *l;

  sw_set_add_from (priv->current_contacts_set, set);
  contacts = sw_set_as_list (set);

  for (l = contacts; l; l = l->next)
  {
    SwContact *contact = (SwContact *)l->data;

    g_hash_table_replace (priv->uid_to_contacts,
                          g_strdup (sw_contact_get (contact, "id")),
                          g_object_ref (contact));
  }

  sw_contact_view_add_contacts (contact_view, contacts);
  g_list_free (contacts);
}

/* TODO: Export this function ? */
/**
 * sw_contact_view_remove_from_set
 * @contact_view: A #SwContactView
 * @set: A #SwSet
 *
 * Remove the contacts that are in the supplied set from the view.
 *
 * This is used in the implementation of sw_contact_view_set_from_set()
 */
void
sw_contact_view_remove_from_set (SwContactView *contact_view,
                              SwSet      *set)
{
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);
  GList *contacts;
  GList *l;

  sw_set_remove_from (priv->current_contacts_set, set);

  contacts = sw_set_as_list (set);

  for (l = contacts; l; l = l->next)
  {
    SwContact *contact = (SwContact *)l->data;

    g_hash_table_remove (priv->uid_to_contacts,
                         sw_contact_get (contact, "id"));
  }

  sw_contact_view_remove_contacts (contact_view, contacts);
  g_list_free (contacts);
}

/**
 * sw_contact_view_update_existing
 * @contact_view: A #SwContactView
 * @set: A #SwSet
 *
 * Replaces contacts in the internal set for the #SwContactView with the version
 * from #SwSet if and only if they are sw_contact_equal() says that they are
 * unequal. This prevents sending excessive contacts changed signals.
 */
static void
sw_contact_view_update_existing (SwContactView *contact_view,
                              SwSet      *set)
{
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);
  GList *contacts;
  SwContact *new_contact;
  SwContact *old_contact;
  GList *l;
  GList *contacts_to_send = NULL;

  contacts = sw_set_as_list (set);

  for (l = contacts; l; l = l->next)
  {
    new_contact = (SwContact *)l->data;
    old_contact = g_hash_table_lookup (priv->uid_to_contacts,
                                    sw_contact_get (new_contact, "id"));

    /* This is just a new contact so we won't find it */
    if (!old_contact)
      continue;

    if (!sw_contact_equal (new_contact, old_contact))
    {
      g_hash_table_replace (priv->uid_to_contacts,
                            g_strdup (sw_contact_get (new_contact, "id")),
                            new_contact);
      /* 
       * This works because sw_set_add uses g_hash_table_replace behind the
       * scenes
       */
      sw_set_add (priv->current_contacts_set, (GObject *)new_contact);
      contacts_to_send = g_list_append (contacts_to_send, g_object_ref (new_contact));
    }
  }

  sw_contact_view_update_contacts (contact_view, contacts_to_send);

  g_list_free (contacts);
}

/**
 * sw_contact_view_set_from_set
 * @contact_view: A #SwContactView
 * @set: A #SwSet
 *
 * Updates what the view contains based on the given #SwSet. Removed
 * signals will be fired for any contacts that were in the view but that are not
 * present in the supplied set. Conversely any contacts that are new will cause
 * signals to be fired indicating their addition.
 *
 * This implemented by maintaining a set inside the #SwContactView
 */
void
sw_contact_view_set_from_set (SwContactView *contact_view,
                           SwSet      *set)
{
  SwContactViewPrivate *priv = GET_PRIVATE (contact_view);
  SwSet *added_contacts, *removed_contacts;

  if (sw_set_is_empty (priv->current_contacts_set))
  {
    sw_contact_view_add_from_set (contact_view, set);
  } else {
    removed_contacts = sw_set_difference (priv->current_contacts_set, set);
    added_contacts = sw_set_difference (set, priv->current_contacts_set);

    if (!sw_set_is_empty (removed_contacts))
      sw_contact_view_remove_from_set (contact_view, removed_contacts);

    /* 
     * Replace contacts that exist in the new set that are also present in the
     * original set iff they're not equal
     *
     * This function will also cause the ContactsChanged signal to be fired with
     * the contacts that have changed.
     */
    sw_contact_view_update_existing (contact_view, set);

    if (!sw_set_is_empty (added_contacts))
      sw_contact_view_add_from_set (contact_view, added_contacts);

    sw_set_unref (removed_contacts);
    sw_set_unref (added_contacts);
  }
}
