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

#include "mojito-client-item-view.h"

#include <interfaces/mojito-item-view-bindings.h>
#include <interfaces/mojito-marshals.h>

G_DEFINE_TYPE (MojitoClientItemView, mojito_client_item_view, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_CLIENT_ITEM_VIEW, MojitoClientItemViewPrivate))

typedef struct _MojitoClientItemViewPrivate MojitoClientItemViewPrivate;

struct _MojitoClientItemViewPrivate {
    gchar *object_path;
    DBusGConnection *connection;
    DBusGProxy *proxy;
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
#define MOJITO_SERVICE_ITEM_VIEW_INTERFACE "com.intel.Mojito.ItemView"

static void
mojito_client_item_view_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  MojitoClientItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_OBJECT_PATH:
      g_value_set_string (value, priv->object_path);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_client_item_view_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  MojitoClientItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_OBJECT_PATH:
      priv->object_path = g_value_dup_string (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_client_item_view_dispose (GObject *object)
{
  MojitoClientItemViewPrivate *priv = GET_PRIVATE (object);

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

  G_OBJECT_CLASS (mojito_client_item_view_parent_class)->dispose (object);
}

static void
mojito_client_item_view_finalize (GObject *object)
{
  MojitoClientItemViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->object_path);

  G_OBJECT_CLASS (mojito_client_item_view_parent_class)->finalize (object);
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
  g_debug (G_STRLOC ": Created item: %s on %s", item->uuid, item->service);

  return item;
}

static void
_proxy_items_added_cb (DBusGProxy *proxy,
                       GPtrArray  *items,
                       gpointer    userdata)
{
  MojitoClientItemView *item_view = MOJITO_CLIENT_ITEM_VIEW (userdata);
  gint i = 0;
  GList *items_list = NULL;

  for (i = 0; i < items->len; i++)
  {
    GValueArray *varray = (GValueArray *)g_ptr_array_index (items, i);
    MojitoItem *item;

    item = _mojito_item_from_value_array (varray);
    items_list = g_list_append (items_list, item);
  }

  g_signal_emit (item_view, signals[ITEMS_ADDED_SIGNAL], 0, items_list);
}

static void
_proxy_items_changed_cb (DBusGProxy *proxy,
                         GPtrArray  *items,
                         gpointer    userdata)
{
  gint i = 0;
  GList *items_list = NULL;

  for (i = 0; i < items->len; i++)
  {
    GValueArray *varray = (GValueArray *)g_ptr_array_index (items, i);
    MojitoItem *item;

    item = _mojito_item_from_value_array (varray);
    items_list = g_list_append (items_list, item);
  }
}

static void
_proxy_items_removed_cb (DBusGProxy *proxy,
                         GPtrArray  *items,
                         gpointer    userdata)
{
  gint i= 0;
  GList *items_list = NULL;

  for (i = 0; i < items->len; i++)
  {
    GValueArray *varray = (GValueArray *)g_ptr_array_index (items, i);
    MojitoItem *item;

    item = _mojito_item_from_value_array (varray);
    items_list = g_list_append (items_list, item);
  }
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

static void
mojito_client_item_view_constructed (GObject *object)
{
  MojitoClientItemViewPrivate *priv = GET_PRIVATE (object);
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
                                                 MOJITO_SERVICE_NAME,
                                                 priv->object_path,
                                                 MOJITO_SERVICE_ITEM_VIEW_INTERFACE,
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
                           _mojito_items_get_container_type (),
                           NULL);
  dbus_g_proxy_connect_signal (priv->proxy,
                               "ItemsRemoved",
                               (GCallback)_proxy_items_removed_cb,
                               object,
                               NULL);
}

static void
mojito_client_item_view_class_init (MojitoClientItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MojitoClientItemViewPrivate));

  object_class->get_property = mojito_client_item_view_get_property;
  object_class->set_property = mojito_client_item_view_set_property;
  object_class->dispose = mojito_client_item_view_dispose;
  object_class->finalize = mojito_client_item_view_finalize;
  object_class->constructed = mojito_client_item_view_constructed;

  pspec = g_param_spec_string ("object-path",
                               "Object path",
                               "DBUS path to the item_view's object",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_OBJECT_PATH, pspec);

  signals[ITEMS_ADDED_SIGNAL] =
    g_signal_new ("items-added",
                  MOJITO_TYPE_CLIENT_ITEM_VIEW,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MojitoClientItemViewClass, items_added),
                  NULL,
                  NULL,
                  mojito_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);
  signals[ITEMS_REMOVED_SIGNAL] =
    g_signal_new ("items-removed",
                  MOJITO_TYPE_CLIENT_ITEM_VIEW,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MojitoClientItemViewClass, items_removed),
                  NULL,
                  NULL,
                  mojito_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);
  signals[ITEMS_CHANGED_SIGNAL] =
    g_signal_new ("items-changed",
                  MOJITO_TYPE_CLIENT_ITEM_VIEW,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MojitoClientItemViewClass, items_changed),
                  NULL,
                  NULL,
                  mojito_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);

}

static void
mojito_client_item_view_init (MojitoClientItemView *self)
{
}

MojitoClientItemView *
_mojito_client_item_view_new_for_path (const gchar *item_view_path)
{
  return g_object_new (MOJITO_TYPE_CLIENT_ITEM_VIEW,
                       "object-path", item_view_path,
                       NULL);
}

static void
_mojito_client_item_view_start_cb (DBusGProxy *proxy,
                                   GError     *error,
                                   gpointer    userdata)
{
  if (error)
  {
    g_warning (G_STRLOC ": Error when starting item_view: %s",
               error->message);
    g_error_free (error);
  }
}

void
mojito_client_item_view_start (MojitoClientItemView *item_view)
{
  MojitoClientItemViewPrivate *priv = GET_PRIVATE (item_view);

  com_intel_Mojito_ItemView_start_async (priv->proxy,
                                         _mojito_client_item_view_start_cb,
                                         NULL);
}


static void
_mojito_client_item_view_refresh_cb (DBusGProxy *proxy,
                                     GError     *error,
                                     gpointer    userdata)
{
  if (error)
  {
    g_warning (G_STRLOC ": Error when refreshing item_view: %s",
               error->message);
    g_error_free (error);
  }
}

void
mojito_client_item_view_refresh (MojitoClientItemView *item_view)
{
  MojitoClientItemViewPrivate *priv = GET_PRIVATE (item_view);

  com_intel_Mojito_ItemView_refresh_async (priv->proxy,
                                           _mojito_client_item_view_refresh_cb,
                                           NULL);
}

