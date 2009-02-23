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
#include "mojito-client.h"
#include "mojito-client-service-private.h"
#include "mojito-core-bindings.h"

G_DEFINE_TYPE (MojitoClient, mojito_client, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_CLIENT, MojitoClientPrivate))

typedef struct _MojitoClientPrivate MojitoClientPrivate;

struct _MojitoClientPrivate {
  DBusGConnection *connection;
  DBusGProxy *proxy;
};

#define MOJITO_SERVICE_NAME "com.intel.Mojito"
#define MOJITO_SERVICE_CORE_OBJECT "/com/intel/Mojito"
#define MOJITO_SERVICE_CORE_INTERFACE "com.intel.Mojito"

static void
mojito_client_dispose (GObject *object)
{
  MojitoClientPrivate *priv = GET_PRIVATE (object);

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

  G_OBJECT_CLASS (mojito_client_parent_class)->dispose (object);
}

static void
mojito_client_finalize (GObject *object)
{
  G_OBJECT_CLASS (mojito_client_parent_class)->finalize (object);
}

static void
mojito_client_class_init (MojitoClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoClientPrivate));

  object_class->dispose = mojito_client_dispose;
  object_class->finalize = mojito_client_finalize;
}


static void
mojito_client_init (MojitoClient *self)
{
  MojitoClientPrivate *priv = GET_PRIVATE (self);
  GError *error = NULL;
  DBusConnection *conn;
  DBusError derror;

  priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

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
                                                 MOJITO_SERVICE_CORE_OBJECT,
                                                 MOJITO_SERVICE_CORE_INTERFACE,
                                                 &error);

  if (!priv->proxy)
  {
    g_critical (G_STRLOC ": Error setting up proxy for remote object: %s",
                error->message);
    g_clear_error (&error);
  }
}

MojitoClient *
mojito_client_new (void)
{
  return g_object_new (MOJITO_TYPE_CLIENT, NULL);
}

typedef struct
{
  MojitoClient *client;
  MojitoClientOpenViewCallback cb;
  gpointer userdata;
} OpenViewClosure;

static void
_mojito_client_open_view_cb (DBusGProxy *proxy,
                             gchar      *view_path,
                             GError     *error,
                             gpointer    userdata)
{
  OpenViewClosure *closure = (OpenViewClosure *)userdata;
  MojitoClient *client = closure->client;
  MojitoClientView *view;

  view = _mojito_client_view_new_for_path (view_path);

  closure->cb (client, view, closure->userdata);

  g_free (view_path);

  g_object_unref (closure->client);
  g_free (closure);
}

void
mojito_client_open_view (MojitoClient                *client,
                         GList                       *services,
                         guint                        count,
                         MojitoClientOpenViewCallback cb,
                         gpointer                     userdata)
{
  MojitoClientPrivate *priv = GET_PRIVATE (client);
  GPtrArray *services_array;
  GList *l;
  OpenViewClosure *closure;

  services_array = g_ptr_array_new ();

  for (l = services; l; l = l->next)
  {
    g_ptr_array_add (services_array, l->data);
  }

  g_ptr_array_add (services_array, NULL);

  closure = g_new0 (OpenViewClosure, 1);
  closure->client = g_object_ref (client);
  closure->cb = cb;
  closure->userdata = userdata;

  com_intel_Mojito_open_view_async (priv->proxy,
                                    (const gchar **)services_array->pdata,
                                    count,
                                    _mojito_client_open_view_cb,
                                    closure);

  g_ptr_array_free (services_array, TRUE);
}

void
mojito_client_open_view_for_service (MojitoClient                 *client,
                                     const gchar                  *service_name,
                                     guint                         count,
                                     MojitoClientOpenViewCallback  cb,
                                     gpointer                      userdata)
{
  MojitoClientPrivate *priv = GET_PRIVATE (client);
  GPtrArray *services_array;
  OpenViewClosure *closure;

  services_array = g_ptr_array_new ();

  g_ptr_array_add (services_array, (gchar *)service_name);
  g_ptr_array_add (services_array, NULL);

  closure = g_new0 (OpenViewClosure, 1);
  closure->client = g_object_ref (client);
  closure->cb = cb;
  closure->userdata = userdata;

  com_intel_Mojito_open_view_async (priv->proxy,
                                    (const gchar **)services_array->pdata,
                                    count,
                                    _mojito_client_open_view_cb,
                                    closure);

  g_ptr_array_free (services_array, TRUE);
}

typedef struct
{
  MojitoClient *client;
  MojitoClientGetServicesCallback cb;
  gpointer userdata;
} GetServicesClosure;

static void
_mojito_client_get_services_cb (DBusGProxy *proxy,
                                gchar     **services,
                                GError     *error,
                                gpointer    userdata)
{
  GetServicesClosure *closure = (GetServicesClosure *)userdata;
  MojitoClient *client = closure->client;

  GList *services_list = NULL;
  gchar *service;
  gint i;

  for (i = 0; (service = services[i]); i++)
  {
    services_list = g_list_append (services_list, service);
  }

  closure->cb (client, services_list, closure->userdata);

  g_list_free (services_list);
  g_strfreev (services);

  g_object_unref (closure->client);
  g_free (closure);
}

void 
mojito_client_get_services (MojitoClient                   *client,
                            MojitoClientGetServicesCallback cb,
                            gpointer                        userdata)
{
  MojitoClientPrivate *priv = GET_PRIVATE (client);
  GetServicesClosure *closure;

  closure = g_new0 (GetServicesClosure, 1);
  closure->client = g_object_ref (client);
  closure->cb = cb;
  closure->userdata = userdata;

  com_intel_Mojito_get_services_async (priv->proxy,
                                       _mojito_client_get_services_cb,
                                       closure);
}

MojitoClientService *
mojito_client_get_service (MojitoClient *client,
                           const gchar  *service_name)
{
  MojitoClientService *service;
  GError *error = NULL;
  service = g_object_new (MOJITO_CLIENT_TYPE_SERVICE,
                          NULL);
  if (!_mojito_client_service_setup_proxy (service, service_name, &error))
  {
    g_warning (G_STRLOC ": Error setting up proxy: %s",
               error->message);
    g_clear_error (&error);
    g_object_unref (service);
    return NULL;
  }

  return service;
}

