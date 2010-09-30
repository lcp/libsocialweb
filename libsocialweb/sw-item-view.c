/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 *
 * Author: Rob Bradford <rob@linux.intel.com>
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
#include "sw-item-view.h"
#include "sw-item-view-ginterface.h"

#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-core.h>

static void sw_item_view_iface_init (gpointer g_iface, gpointer iface_data);
G_DEFINE_TYPE_WITH_CODE (SwItemView, sw_item_view, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SW_TYPE_ITEM_VIEW_IFACE,
                                                sw_item_view_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_ITEM_VIEW, SwItemViewPrivate))

typedef struct _SwItemViewPrivate SwItemViewPrivate;

struct _SwItemViewPrivate {
  SwService *service;
  gchar *object_path;
  SwSet *current_items_set;
  SwSet *pending_items_set;

  /* timeout used for coalescing multiple delayed ready additions */
  guint pending_timeout_id;

  /* timeout used for ratelimiting checking for changed items */
  guint refresh_timeout_id;

  GHashTable *uid_to_items;

  GList *changed_items;
};

enum
{
  PROP_0,
  PROP_SERVICE,
  PROP_OBJECT_PATH
};

#if 0
static void sw_item_view_add_item (SwItemView *item_view,
                                   SwItem     *item);
static void sw_item_view_update_item (SwItemView *item_view,
                                      SwItem     *item);
static void sw_item_view_remove_item (SwItemView *item_view,
                                      SwItem     *item);
#endif

static void sw_item_view_add_items (SwItemView *item_view,
                                    GList      *items);
static void sw_item_view_update_items (SwItemView *item_view,
                                       GList      *items);
static void sw_item_view_remove_items (SwItemView *item_view,
                                       GList      *items);

static void
sw_item_view_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  SwItemViewPrivate *priv = GET_PRIVATE (object);

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
sw_item_view_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  SwItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_SERVICE:
      priv->service = g_value_dup_object (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_item_view_dispose (GObject *object)
{
  SwItemViewPrivate *priv = GET_PRIVATE (object);

  if (priv->service)
  {
    g_object_unref (priv->service);
    priv->service = NULL;
  }

  if (priv->current_items_set)
  {
    sw_set_unref (priv->current_items_set);
    priv->current_items_set = NULL;
  }

  if (priv->pending_items_set)
  {
    sw_set_unref (priv->pending_items_set);
    priv->pending_items_set = NULL;
  }

  if (priv->uid_to_items)
  {
    g_hash_table_unref (priv->uid_to_items);
    priv->uid_to_items = NULL;
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

  G_OBJECT_CLASS (sw_item_view_parent_class)->dispose (object);
}

static void
sw_item_view_finalize (GObject *object)
{
  SwItemViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->object_path);

  G_OBJECT_CLASS (sw_item_view_parent_class)->finalize (object);
}

static gchar *
_make_object_path (SwItemView *item_view)
{
  gchar *path;
  static gint count = 0;

  path = g_strdup_printf ("/com/meego/libsocialweb/View%d",
                          count);

  count++;

  return path;
}

static void
sw_item_view_constructed (GObject *object)
{
  SwItemView *item_view = SW_ITEM_VIEW (object);
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwCore *core;

  core = sw_core_dup_singleton ();

  priv->object_path = _make_object_path (item_view);
  dbus_g_connection_register_g_object (sw_core_get_connection (core),
                                       priv->object_path,
                                       G_OBJECT (item_view));
  g_object_unref (core);
  /* The only reference should be the one on the bus */

  if (G_OBJECT_CLASS (sw_item_view_parent_class)->constructed)
    G_OBJECT_CLASS (sw_item_view_parent_class)->constructed (object);
}

/* Default implementation for close */
static void
sw_item_view_default_close (SwItemView *item_view)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwCore *core;

  SW_DEBUG (VIEWS, "%s called on %s", G_STRFUNC, priv->object_path);

  core = sw_core_dup_singleton ();
  dbus_g_connection_unregister_g_object (sw_core_get_connection (core),
                                         G_OBJECT (item_view));
  g_object_unref (core);

  /* Object is no longer needed */
  g_object_unref (item_view);
}

static void
sw_item_view_class_init (SwItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwItemViewPrivate));

  object_class->get_property = sw_item_view_get_property;
  object_class->set_property = sw_item_view_set_property;
  object_class->dispose = sw_item_view_dispose;
  object_class->finalize = sw_item_view_finalize;
  object_class->constructed = sw_item_view_constructed;

  klass->close = sw_item_view_default_close;

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
sw_item_view_init (SwItemView *self)
{
  SwItemViewPrivate *priv = GET_PRIVATE (self);

  priv->current_items_set = sw_item_set_new ();
  priv->pending_items_set = sw_item_set_new ();

  priv->uid_to_items = g_hash_table_new_full (g_str_hash,
                                              g_str_equal,
                                              g_free,
                                              g_object_unref);
}

/* DBUS interface to class vfunc bindings */

static void
sw_item_view_start (SwItemViewIface       *iface,
                    DBusGMethodInvocation *context)
{
  SwItemView *item_view = SW_ITEM_VIEW (iface);
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);

  SW_DEBUG (VIEWS, "%s called on %s", G_STRFUNC, priv->object_path);

  if (SW_ITEM_VIEW_GET_CLASS (iface)->start)
    SW_ITEM_VIEW_GET_CLASS (iface)->start (item_view);

  sw_item_view_iface_return_from_start (context);
}

static void
sw_item_view_refresh (SwItemViewIface       *iface,
                      DBusGMethodInvocation *context)
{
  SwItemView *item_view = SW_ITEM_VIEW (iface);
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);

  SW_DEBUG (VIEWS, "%s called on %s", G_STRFUNC, priv->object_path);

  if (SW_ITEM_VIEW_GET_CLASS (iface)->refresh)
    SW_ITEM_VIEW_GET_CLASS (iface)->refresh (item_view);

  sw_item_view_iface_return_from_refresh (context);
}

static void
sw_item_view_stop (SwItemViewIface       *iface,
                   DBusGMethodInvocation *context)
{
  SwItemView *item_view = SW_ITEM_VIEW (iface);
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);

  SW_DEBUG (VIEWS, "%s called on %s", G_STRFUNC, priv->object_path);

  if (SW_ITEM_VIEW_GET_CLASS (iface)->stop)
    SW_ITEM_VIEW_GET_CLASS (iface)->stop (item_view);

  sw_item_view_iface_return_from_stop (context);
}

static void
sw_item_view_close (SwItemViewIface       *iface,
                    DBusGMethodInvocation *context)
{
  SwItemView *item_view = SW_ITEM_VIEW (iface);
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);

  SW_DEBUG (VIEWS, "%s called on %s", G_STRFUNC, priv->object_path);

  if (SW_ITEM_VIEW_GET_CLASS (iface)->close)
    SW_ITEM_VIEW_GET_CLASS (iface)->close (item_view);

  sw_item_view_iface_return_from_close (context);
}

static void
sw_item_view_iface_init (gpointer g_iface,
                         gpointer iface_data)
{
  SwItemViewIfaceClass *klass = (SwItemViewIfaceClass*)g_iface;
  sw_item_view_iface_implement_start (klass, sw_item_view_start);
  sw_item_view_iface_implement_refresh (klass, sw_item_view_refresh);
  sw_item_view_iface_implement_stop (klass, sw_item_view_stop);
  sw_item_view_iface_implement_close (klass, sw_item_view_close);
}

static gboolean
_handle_ready_pending_cb (gpointer data)
{
  SwItemView *item_view = SW_ITEM_VIEW (data);
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);
  GList *items_to_send = NULL;
  GList *pending_items, *l;

  SW_DEBUG (VIEWS, "Delayed ready timeout fired");

  /* FIXME: Reword this to avoid unnecessary list creation ? */
  pending_items = sw_set_as_list (priv->pending_items_set);

  for (l = pending_items; l; l = l->next)
  {
    SwItem *item = SW_ITEM (l->data);

    if (sw_item_get_ready (item))
    {
      items_to_send = g_list_prepend (items_to_send, item);
      sw_set_remove (priv->pending_items_set, (GObject *)item);
    }
  }

  sw_item_view_add_items (item_view, items_to_send);

  g_list_free (pending_items);

  priv->pending_timeout_id = 0;

  return FALSE;
}

static void
_item_ready_weak_notify_cb (gpointer  data,
                            GObject  *dead_object);

static void
_item_ready_notify_cb (SwItem     *item,
                       GParamSpec *pspec,
                       SwItemView *item_view)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (sw_item_get_ready (item)) {
    SW_DEBUG (VIEWS, "Item became ready: %s.",
              sw_item_get (item, "id"));
    g_signal_handlers_disconnect_by_func (item,
                                          _item_ready_notify_cb,
                                          item_view);
    g_object_weak_unref ((GObject *)item_view,
                         _item_ready_weak_notify_cb,
                         item);

    if (!priv->pending_timeout_id)
    {
      SW_DEBUG (VIEWS, "Setting up timeout");
      priv->pending_timeout_id = g_timeout_add_seconds (1,
                                                        _handle_ready_pending_cb,
                                                        item_view);
    } else {
      SW_DEBUG (VIEWS, "Timeout already set up.");
    }
  }
}

static void
_item_ready_weak_notify_cb (gpointer  data,
                            GObject  *dead_object)
{
  g_signal_handlers_disconnect_by_func (data,
                                        _item_ready_notify_cb,
                                        dead_object);
}

static void
_setup_ready_handler (SwItem     *item,
                      SwItemView *item_view)
{
  g_signal_connect (item,
                    "notify::ready",
                    (GCallback)_item_ready_notify_cb,
                    item_view);
  g_object_weak_ref ((GObject *)item_view,
                     _item_ready_weak_notify_cb,
                     item);
}

static gboolean
_item_changed_timeout_cb (gpointer data)
{
  SwItemView *item_view = SW_ITEM_VIEW (data);
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);

  sw_item_view_update_items (item_view, priv->changed_items);
  g_list_foreach (priv->changed_items, (GFunc)g_object_unref, NULL);
  g_list_free (priv->changed_items);
  priv->changed_items = NULL;

  priv->refresh_timeout_id = 0;

  return FALSE;
}

static void
_item_changed_cb (SwItem     *item,
                  SwItemView *item_view)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);

  /* We only care if the item is ready. If it's not then we don't want to be
   * emitting changed but instead it will be added through the readiness
   * tracking.
   */
  if (!sw_item_get_ready (item))
    return;

  if (!g_list_find (priv->changed_items, item))
    priv->changed_items = g_list_append (priv->changed_items, item);

  if (!priv->refresh_timeout_id)
  {
    SW_DEBUG (VIEWS, "Item changed, Setting up timeout");

    priv->refresh_timeout_id = g_timeout_add_seconds (10,
                                                      _item_changed_timeout_cb,
                                                      item_view);
  }
}

static void
_item_changed_weak_notify_cb (gpointer  data,
                              GObject  *dead_object)
{
  SwItem *item = (SwItem *)data;

  g_signal_handlers_disconnect_by_func (item,
                                        _item_changed_cb,
                                        dead_object);
  g_object_unref (item);
}

static void
_setup_changed_handler (SwItem     *item,
                        SwItemView *item_view)
{
  g_signal_connect (item,
                    "changed",
                    (GCallback)_item_changed_cb,
                    item_view);
  g_object_weak_ref ((GObject *)item_view,
                     _item_changed_weak_notify_cb,
                     g_object_ref (item));
}

/* FIXME: Do we need these functions still? */
#if 0
/**
 * sw_item_view_add_item
 * @item_view: A #SwItemView
 * @item: A #SwItem
 *
 * Add a single item in the #SwItemView. This will cause a signal to be
 * emitted across the bus.
 */
static void
sw_item_view_add_item (SwItemView *item_view,
                       SwItem     *item)
{
  GList *tmp_list = NULL;

  tmp_list = g_list_append (tmp_list, item);
  sw_item_view_add_items (item_view, tmp_list);
  g_list_free (tmp_list);
}

/**
 * sw_item_view_update_item
 * @item_view: A #SwItemView
 * @item: A #SwItem
 *
 * Update a single item in the #SwItemView. This will cause a signal to be
 * emitted across the bus.
 */
void
sw_item_view_update_item (SwItemView *item_view,
                          SwItem     *item)
{
  GList *tmp_list = NULL;

  tmp_list = g_list_append (tmp_list, item);
  sw_item_view_update_items (item_view, tmp_list);
  g_list_free (tmp_list);
}

/**
 * sw_item_view_remove_item
 * @item_view: A #SwItemView
 * @item: A #SwItem
 *
 * Remove a single item to the #SwItemView. This will cause a signal to be
 * emitted across the bus.
 */
void
sw_item_view_remove_item (SwItemView *item_view,
                          SwItem     *item)
{
  GList *tmp_list = NULL;

  tmp_list = g_list_append (tmp_list, item);
  sw_item_view_remove_items (item_view, tmp_list);
  g_list_free (tmp_list);
}
#endif

/**
 * sw_item_view_add_items
 * @item_view: A #SwItemView
 * @items: A list of #SwItem objects
 *
 * Add the items supplied in the list from the #SwItemView. In many
 * cases what you actually want is sw_item_view_remove_from_set() or
 * sw_item_view_set_from_set(). This will cause signal emissions over the
 * bus.
 *
 * This is used in the implementation of sw_item_view_remove_from_set()
 */
static void
sw_item_view_add_items (SwItemView *item_view,
                        GList      *items)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);
  GValueArray *value_array;
  GPtrArray *ptr_array;
  GList *l;

  ptr_array = g_ptr_array_new_with_free_func ((GDestroyNotify)g_value_array_free);

  for (l = items; l; l = l->next)
  {
    SwItem *item = SW_ITEM (l->data);

    if (sw_item_get_ready (item))
    {
      SW_DEBUG (VIEWS, "Item ready: %s",
                sw_item_get (item, "id"));
      value_array = _sw_item_to_value_array (item);
      g_ptr_array_add (ptr_array, value_array);
    } else {
      SW_DEBUG (VIEWS, "Item not ready, setting up handler: %s",
                sw_item_get (item, "id"));
      _setup_ready_handler (item, item_view);
      sw_set_add (priv->pending_items_set, (GObject *)item);
    }

    _setup_changed_handler (item, item_view);
  }

  SW_DEBUG (VIEWS, "Number of items to be added: %d", ptr_array->len);

  sw_item_view_iface_emit_items_added (item_view,
                                       ptr_array);

  g_ptr_array_free (ptr_array, TRUE);
}

/**
 * sw_item_view_update_items
 * @item_view: A #SwItemView
 * @items: A list of #SwItem objects that need updating
 *
 * Update the items supplied in the list in the #SwItemView. This is
 * will cause signal emissions over the bus.
 */
static void
sw_item_view_update_items (SwItemView *item_view,
                           GList      *items)
{
  GValueArray *value_array;
  GPtrArray *ptr_array;
  GList *l;

  ptr_array = g_ptr_array_new_with_free_func ((GDestroyNotify)g_value_array_free);

  for (l = items; l; l = l->next)
  {
    SwItem *item = SW_ITEM (l->data);

    /*
     * Item must be ready and also not in the pending items set; we need to
     * check this to prevent ItemsChanged coming before ItemsAdded
     */
    if (sw_item_get_ready (item))
    {
      value_array = _sw_item_to_value_array (item);
      g_ptr_array_add (ptr_array, value_array);
    }
  }

  SW_DEBUG (VIEWS, "Number of items to be changed: %d", ptr_array->len);

  if (ptr_array->len > 0)
    sw_item_view_iface_emit_items_changed (item_view,
                                           ptr_array);
  g_ptr_array_free (ptr_array, TRUE);
}

/**
 * sw_item_view_remove_items
 * @item_view: A #SwItemView
 * @items: A list of #SwItem objects
 *
 * Remove the items supplied in the list from the #SwItemView. In many
 * cases what you actually want is sw_item_view_remove_from_set() or
 * sw_item_view_set_from_set(). This will cause signal emissions over the
 * bus.
 *
 * This is used in the implementation of sw_item_view_remove_from_set()
 *
 */
static void
sw_item_view_remove_items (SwItemView *item_view,
                           GList      *items)
{
  GValueArray *value_array;
  GPtrArray *ptr_array;
  GList *l;
  SwItem *item;

  ptr_array = g_ptr_array_new_with_free_func ((GDestroyNotify)g_value_array_free);

  for (l = items; l; l = l->next)
  {
    item = SW_ITEM (l->data);

    value_array = g_value_array_new (2);

    value_array = g_value_array_append (value_array, NULL);
    g_value_init (g_value_array_get_nth (value_array, 0), G_TYPE_STRING);
    g_value_set_string (g_value_array_get_nth (value_array, 0),
                        sw_service_get_name (sw_item_get_service (item)));

    value_array = g_value_array_append (value_array, NULL);
    g_value_init (g_value_array_get_nth (value_array, 1), G_TYPE_STRING);
    g_value_set_string (g_value_array_get_nth (value_array, 1),
                        sw_item_get (item, "id"));

    g_ptr_array_add (ptr_array, value_array);
  }

  sw_item_view_iface_emit_items_removed (item_view,
                                         ptr_array);

  g_ptr_array_free (ptr_array, TRUE);
}

/**
 * sw_item_view_get_object_path
 * @item_view: A #SwItemView
 *
 * Since #SwItemView is responsible for constructing the object path and
 * registering the object on the bus. This function is necessary for
 * #SwCore to be able to return the object path as the result of a
 * function to open a view.
 *
 * Returns: A string providing the object path.
 */
const gchar *
sw_item_view_get_object_path (SwItemView *item_view)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);

  return priv->object_path;
}

/**
 * sw_item_view_get_service
 * @item_view: A #SwItemView
 *
 * Returns: The #SwService that #SwItemView is for
 */
SwService *
sw_item_view_get_service (SwItemView *item_view)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);

  return priv->service;
}

/* TODO: Export this function ? */
/**
 * sw_item_view_add_from_set
 * @item_view: A #SwItemView
 * @set: A #SwSet
 *
 * Add the items that are in the supplied set to the view.
 *
 * This is used in the implementation of sw_item_view_set_from_set()
 */
void
sw_item_view_add_from_set (SwItemView *item_view,
                           SwSet      *set)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);
  GList *items;
  GList *l;

  sw_set_add_from (priv->current_items_set, set);
  items = sw_set_as_list (set);

  for (l = items; l; l = l->next)
  {
    SwItem *item = (SwItem *)l->data;

    g_hash_table_replace (priv->uid_to_items,
                          g_strdup (sw_item_get (item, "id")),
                          g_object_ref (item));
  }

  sw_item_view_add_items (item_view, items);
  g_list_free (items);
}

/* TODO: Export this function ? */
/**
 * sw_item_view_remove_from_set
 * @item_view: A #SwItemView
 * @set: A #SwSet
 *
 * Remove the items that are in the supplied set from the view.
 *
 * This is used in the implementation of sw_item_view_set_from_set()
 */
void
sw_item_view_remove_from_set (SwItemView *item_view,
                              SwSet      *set)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);
  GList *items;
  GList *l;

  sw_set_remove_from (priv->current_items_set, set);

  items = sw_set_as_list (set);

  for (l = items; l; l = l->next)
  {
    SwItem *item = (SwItem *)l->data;

    g_hash_table_remove (priv->uid_to_items,
                         sw_item_get (item, "id"));
  }

  sw_item_view_remove_items (item_view, items);
  g_list_free (items);
}

/**
 * sw_item_view_update_existing
 * @item_view: A #SwItemView
 * @set: A #SwSet
 *
 * Replaces items in the internal set for the #SwItemView with the version
 * from #SwSet if and only if they are sw_item_equal() says that they are
 * unequal. This prevents sending excessive items changed signals.
 */
static void
sw_item_view_update_existing (SwItemView *item_view,
                              SwSet      *set)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);
  GList *items;
  SwItem *new_item;
  SwItem *old_item;
  GList *l;
  GList *items_to_send = NULL;

  items = sw_set_as_list (set);

  for (l = items; l; l = l->next)
  {
    new_item = (SwItem *)l->data;
    old_item = g_hash_table_lookup (priv->uid_to_items,
                                    sw_item_get (new_item, "id"));

    /* This is just a new item so we won't find it */
    if (!old_item)
      continue;

    if (!sw_item_equal (new_item, old_item))
    {
      g_hash_table_replace (priv->uid_to_items,
                            g_strdup (sw_item_get (new_item, "id")),
                            new_item);
      /* 
       * This works because sw_set_add uses g_hash_table_replace behind the
       * scenes
       */
      sw_set_add (priv->current_items_set, (GObject *)new_item);
      items_to_send = g_list_append (items_to_send, g_object_ref (new_item));
    }
  }

  sw_item_view_update_items (item_view, items_to_send);

  g_list_free (items);
}

/**
 * sw_item_view_set_from_set
 * @item_view: A #SwItemView
 * @set: A #SwSet
 *
 * Updates what the view contains based on the given #SwSet. Removed
 * signals will be fired for any items that were in the view but that are not
 * present in the supplied set. Conversely any items that are new will cause
 * signals to be fired indicating their addition.
 *
 * This implemented by maintaining a set inside the #SwItemView
 */
void
sw_item_view_set_from_set (SwItemView *item_view,
                           SwSet      *set)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwSet *added_items, *removed_items;

  if (sw_set_is_empty (priv->current_items_set))
  {
    sw_item_view_add_from_set (item_view, set);
  } else {
    removed_items = sw_set_difference (priv->current_items_set, set);
    added_items = sw_set_difference (set, priv->current_items_set);

    if (!sw_set_is_empty (removed_items))
      sw_item_view_remove_from_set (item_view, removed_items);

    /* 
     * Replace items that exist in the new set that are also present in the
     * original set iff they're not equal
     *
     * This function will also cause the ItemsChanged signal to be fired with
     * the items that have changed.
     */
    sw_item_view_update_existing (item_view, set);

    if (!sw_set_is_empty (added_items))
      sw_item_view_add_from_set (item_view, added_items);

    sw_set_unref (removed_items);
    sw_set_unref (added_items);
  }
}

static void
sw_item_view_remove_item (SwItemView *item_view,
                          SwItem     *item)
{
  GValueArray *value_array;
  GPtrArray *ptr_array;

  ptr_array = g_ptr_array_new ();

  value_array = g_value_array_new (2);

  value_array = g_value_array_append (value_array, NULL);
  g_value_init (g_value_array_get_nth (value_array, 0), G_TYPE_STRING);
  g_value_set_string (g_value_array_get_nth (value_array, 0),
                      sw_service_get_name (sw_item_get_service (item)));

  value_array = g_value_array_append (value_array, NULL);
  g_value_init (g_value_array_get_nth (value_array, 1), G_TYPE_STRING);
  g_value_set_string (g_value_array_get_nth (value_array, 1),
                      sw_item_get (item, "id"));

  g_ptr_array_add (ptr_array, value_array);

  sw_item_view_iface_emit_items_removed (item_view,
                                         ptr_array);

  g_ptr_array_free (ptr_array, TRUE);
}

void
sw_item_view_remove_by_uid (SwItemView  *item_view,
                            const gchar *uid)
{
  SwItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwItem *item;

  item = g_hash_table_lookup (priv->uid_to_items,
                              uid);

  if (item)
  {
    sw_item_view_remove_item (item_view, item);
    sw_set_remove (priv->current_items_set, (GObject *)item);
    g_hash_table_remove (priv->uid_to_items,
                         uid);
  } else {
    g_critical (G_STRLOC ": Asked to remove unknown item: %s", uid);
  }
}
