/*
 * Mojito - social data store
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

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "mojito-client-view.h"

#include <interfaces/mojito-view-bindings.h>
#include <interfaces/mojito-marshals.h>

G_DEFINE_TYPE (MojitoClientView, mojito_client_view, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_CLIENT_VIEW, MojitoClientViewPrivate))

typedef struct _MojitoClientViewPrivate MojitoClientViewPrivate;

struct _MojitoClientViewPrivate {
  gchar *object_path;
  DBusGConnection *connection;
  DBusGProxy *proxy;
  GHashTable *uuid_to_items;
};

enum
{
  PROP_0,
  PROP_OBJECT_PATH
};

enum
{
  ITEMS_ADDED_SIGNAL,
  ITEMS_CHANGED_SIGNAL,
  ITEMS_REMOVED_SIGNAL,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

#define MOJITO_SERVICE_NAME "com.intel.Mojito"
#define MOJITO_SERVICE_VIEW_INTERFACE "com.intel.Mojito.View"

static void
mojito_client_view_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  MojitoClientViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_OBJECT_PATH:
      g_value_set_string (value, priv->object_path);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_client_view_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  MojitoClientViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_OBJECT_PATH:
      priv->object_path = g_value_dup_string (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_client_view_dispose (GObject *object)
{
  MojitoClientViewPrivate *priv = GET_PRIVATE (object);

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

  if (priv->uuid_to_items)
  {
    g_hash_table_unref (priv->uuid_to_items);
    priv->uuid_to_items = NULL;
  }

  G_OBJECT_CLASS (mojito_client_view_parent_class)->dispose (object);
}

static void
mojito_client_view_finalize (GObject *object)
{
  MojitoClientViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->object_path);

  G_OBJECT_CLASS (mojito_client_view_parent_class)->finalize (object);
}

static MojitoItem *
_mojito_item_from_value_array (GValueArray *varray)
{
  MojitoItem *item;

  item = mojito_item_new ();

  item->service = g_value_dup_string (g_value_array_get_nth (varray, 0));
  item->uuid = g_value_dup_string (g_value_array_get_nth (varray, 1));
  item->date.tv_sec = g_value_get_int64 (g_value_array_get_nth (varray, 2));
  item->props = g_value_dup_boxed (g_value_array_get_nth (varray, 3));

  return item;
}

static void
_proxy_items_added_cb (DBusGProxy *proxy,
                       GPtrArray  *items,
                       gpointer    userdata)
{
  MojitoClientView *view = MOJITO_CLIENT_VIEW (userdata);
  MojitoClientViewPrivate *priv = GET_PRIVATE (view);
  gint i = 0;
  GList *items_list = NULL;

  for (i = 0; i < items->len; i++)
  {
    GValueArray *varray = (GValueArray *)g_ptr_array_index (items, i);
    MojitoItem *item;

    /* First reference dropped when list freed */
    item = _mojito_item_from_value_array (varray);

    g_hash_table_insert (priv->uuid_to_items,
                         g_strdup (item->uuid),
                         mojito_item_ref (item));

    items_list = g_list_append (items_list, item);
  }

  /* If handler wants a ref then it should ref it up */
  g_signal_emit (view, signals[ITEMS_ADDED_SIGNAL], 0, items_list);

  g_list_foreach (items_list, (GFunc)mojito_item_unref, NULL);
  g_list_free (items_list);
}

static void
_proxy_items_changed_cb (DBusGProxy *proxy,
                         GPtrArray  *items,
                         gpointer    userdata)
{
  MojitoClientView *view = MOJITO_CLIENT_VIEW (userdata);
  MojitoClientViewPrivate *priv = GET_PRIVATE (view);
  gint i = 0;
  GList *items_list = NULL;

  for (i = 0; i < items->len; i++)
  {
    GValueArray *varray = (GValueArray *)g_ptr_array_index (items, i);
    MojitoItem *item;

    /* First reference dropped when list freed */
    item = _mojito_item_from_value_array (varray);

    g_hash_table_insert (priv->uuid_to_items,
                         g_strdup (item->uuid),
                         mojito_item_ref (item));

    items_list = g_list_append (items_list, item);
  }

  /* If handler wants a ref then it should ref it up */
  g_signal_emit (view, signals[ITEMS_CHANGED_SIGNAL], 0, items_list);

  g_list_foreach (items_list, (GFunc)mojito_item_unref, NULL);
  g_list_free (items_list);
}

static void
_proxy_items_removed_cb (DBusGProxy *proxy,
                         GPtrArray  *items,
                         gpointer    userdata)
{
  MojitoClientView *view = MOJITO_CLIENT_VIEW (userdata);
  MojitoClientViewPrivate *priv = GET_PRIVATE (view);
  gint i = 0;
  GList *items_list = NULL;

  for (i = 0; i < items->len; i++)
  {
    GValueArray *varray = (GValueArray *)g_ptr_array_index (items, i);
    const gchar *uid;
    MojitoItem *item;

    uid = g_value_get_string (g_value_array_get_nth (varray, 1));

    item = g_hash_table_lookup (priv->uuid_to_items,
                                uid);

    if (item)
    {
      /* Must ref up because g_hash_table_remove drops ref */
      items_list = g_list_append (items_list, mojito_item_ref (item));
      g_hash_table_remove (priv->uuid_to_items, uid);
    }
  }

  /* If handler wants a ref then it should ref it up */
  g_signal_emit (view, signals[ITEMS_REMOVED_SIGNAL], 0, items_list);

  g_list_foreach (items_list, (GFunc)mojito_item_unref, NULL);
  g_list_free (items_list);
}

static GType
_mojito_item_get_struct_type (void)
{
  return dbus_g_type_get_struct ("GValueArray",
                                 G_TYPE_STRING,
                                 G_TYPE_STRING,
                                 G_TYPE_INT64,
                                 dbus_g_type_get_map ("GHashTable",
                                                      G_TYPE_STRING,
                                                      G_TYPE_STRING),
                                 G_TYPE_INVALID);
}

static GType
_mojito_items_get_container_type (void)
{
 return dbus_g_type_get_collection ("GPtrArray",
                                    _mojito_item_get_struct_type ());
}

static GType
_mojito_items_removed_get_container_type (void)
{
  return dbus_g_type_get_collection ("GPtrArray",
                                     dbus_g_type_get_struct ("GValueArray",
                                                             G_TYPE_STRING,
                                                             G_TYPE_STRING,
                                                             G_TYPE_INVALID));
}

static void
mojito_client_view_constructed (GObject *object)
{
  MojitoClientViewPrivate *priv = GET_PRIVATE (object);
  GError *error = NULL;
  DBusConnection *conn;
  DBusError derror;

  priv->connection = dbus_g_bus_get (DBUS_BUS_STARTER, &error);

  if (!priv->connection)
  {
    g_critical (G_STRLOC ": Error getting DBUS connection: %s",
                error->message);
    g_clear_error (&error);
    return;
  }

  conn = dbus_g_connection_get_connection (priv->connection);
  dbus_error_init (&derror);
  if (!dbus_bus_start_service_by_name (conn,
                                       MOJITO_SERVICE_NAME,
                                       0, 
                                       NULL, 
                                       &derror))
  {
    g_critical (G_STRLOC ": Error starting mojito service: %s",
                derror.message);
    dbus_error_free (&derror);
    return;
  }

  priv->proxy = dbus_g_proxy_new_for_name_owner (priv->connection,
                                                 MOJITO_SERVICE_NAME,
                                                 priv->object_path,
                                                 MOJITO_SERVICE_VIEW_INTERFACE,
                                                 &error);

  if (!priv->proxy)
  {
    g_critical (G_STRLOC ": Error setting up proxy for remote object: %s",
                error->message);
    g_clear_error (&error);
    return;
  }

  dbus_g_proxy_add_signal (priv->proxy,
                           "ItemsAdded",
                           _mojito_items_get_container_type (),
                           NULL);
  dbus_g_proxy_connect_signal (priv->proxy,
                               "ItemsAdded",
                               (GCallback)_proxy_items_added_cb,
                               object,
                               NULL);

  dbus_g_proxy_add_signal (priv->proxy,
                           "ItemsChanged",
                           _mojito_items_get_container_type (),
                           NULL);
  dbus_g_proxy_connect_signal (priv->proxy,
                               "ItemsChanged",
                               (GCallback)_proxy_items_changed_cb,
                               object,
                               NULL);

  dbus_g_proxy_add_signal (priv->proxy,
                           "ItemsRemoved",
                           _mojito_items_removed_get_container_type (),
                           NULL);
  dbus_g_proxy_connect_signal (priv->proxy,
                               "ItemsRemoved",
                               (GCallback)_proxy_items_removed_cb,
                               object,
                               NULL);
}

static void
mojito_client_view_class_init (MojitoClientViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MojitoClientViewPrivate));

  object_class->get_property = mojito_client_view_get_property;
  object_class->set_property = mojito_client_view_set_property;
  object_class->dispose = mojito_client_view_dispose;
  object_class->finalize = mojito_client_view_finalize;
  object_class->constructed = mojito_client_view_constructed;

  pspec = g_param_spec_string ("object-path",
                               "Object path",
                               "DBUS path to the view's object",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH, pspec);

  signals[ITEMS_ADDED_SIGNAL] =
    g_signal_new ("items-added",
                  MOJITO_TYPE_CLIENT_VIEW,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MojitoClientViewClass, items_added),
                  NULL,
                  NULL,
                  mojito_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);
  signals[ITEMS_REMOVED_SIGNAL] =
    g_signal_new ("items-removed",
                  MOJITO_TYPE_CLIENT_VIEW,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MojitoClientViewClass, items_removed),
                  NULL,
                  NULL,
                  mojito_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);
  signals[ITEMS_CHANGED_SIGNAL] =
    g_signal_new ("items-changed",
                  MOJITO_TYPE_CLIENT_VIEW,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MojitoClientViewClass, items_changed),
                  NULL,
                  NULL,
                  mojito_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);
}

static void
mojito_client_view_init (MojitoClientView *self)
{
  MojitoClientViewPrivate *priv = GET_PRIVATE (self);

  priv->uuid_to_items = g_hash_table_new_full (g_str_hash,
                                               g_str_equal,
                                               g_free,
                                               (GDestroyNotify)mojito_item_unref);
}

MojitoClientView *
_mojito_client_view_new_for_path (const gchar *view_path)
{
  return g_object_new (MOJITO_TYPE_CLIENT_VIEW,
                       "object-path", view_path,
                       NULL);
}

static void
_mojito_client_view_start_cb (DBusGProxy     *proxy,
                              GError         *error,
                              gpointer        userdata)
{
  if (error)
  {
    g_warning (G_STRLOC ": Error when starting view: %s",
               error->message);
    g_error_free (error);
  }
}

void
mojito_client_view_start (MojitoClientView *view)
{
  MojitoClientViewPrivate *priv = GET_PRIVATE (view);

  com_intel_Mojito_View_start_async (priv->proxy,
                                     _mojito_client_view_start_cb,
                                     NULL);
}


static void
_mojito_client_view_refresh_cb (DBusGProxy     *proxy,
                                GError         *error,
                                gpointer        userdata)
{
  if (error)
  {
    g_warning (G_STRLOC ": Error when refreshing view: %s",
               error->message);
    g_error_free (error);
  }
}

void
mojito_client_view_refresh (MojitoClientView *view)
{
  MojitoClientViewPrivate *priv = GET_PRIVATE (view);

  com_intel_Mojito_View_refresh_async (priv->proxy,
                                       _mojito_client_view_refresh_cb,
                                       NULL);
}


static gint
_sort_compare_func (MojitoItem *a,
                    MojitoItem *b)
{
  if (a->date.tv_sec> b->date.tv_sec)
  {
    return -1;
  } else if (a->date.tv_sec == b->date.tv_sec) {
    return 0;
  } else {
    return 1;
  }
}

GList *
mojito_client_view_get_sorted_items (MojitoClientView *view)
{
  MojitoClientViewPrivate *priv = GET_PRIVATE (view);
  GList *items;

  items = g_hash_table_get_values (priv->uuid_to_items);
  items = g_list_sort (items, (GCompareFunc)_sort_compare_func);

  return items;
}

