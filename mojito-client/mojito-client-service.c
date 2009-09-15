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

#include "mojito-client-service.h"
#include "mojito-service-bindings.h"
#include "mojito-client-marshals.h"

G_DEFINE_TYPE (MojitoClientService, mojito_client_service, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_CLIENT_TYPE_SERVICE, MojitoClientServicePrivate))

typedef struct _MojitoClientServicePrivate MojitoClientServicePrivate;

struct _MojitoClientServicePrivate {
  char *name;
  DBusGConnection *connection;
  DBusGProxy *proxy;
};

enum
{
  AVATAR_RETRIEVED_SIGNAL,
  CAPS_CHANGED_SIGNAL,
  STATUS_UPDATED_SIGNAL,
  USER_CHANGED_SIGNAL,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

#define MOJITO_CLIENT_SERVICE_NAME "com.intel.Mojito"
#define MOJITO_CLIENT_SERVICE_OBJECT "/com/intel/Mojito/Service/%s"
#define MOJITO_CLIENT_SERVICE_INTERFACE "com.intel.Mojito.Service"

static void
mojito_client_service_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  switch (property_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_client_service_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  switch (property_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_client_service_dispose (GObject *object)
{
  MojitoClientServicePrivate *priv = GET_PRIVATE (object);

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

  G_OBJECT_CLASS (mojito_client_service_parent_class)->dispose (object);
}

static void
mojito_client_service_finalize (GObject *object)
{
  MojitoClientServicePrivate *priv = GET_PRIVATE (object);

  g_free (priv->name);

  G_OBJECT_CLASS (mojito_client_service_parent_class)->finalize (object);
}

static void
mojito_client_service_class_init (MojitoClientServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoClientServicePrivate));

  object_class->get_property = mojito_client_service_get_property;
  object_class->set_property = mojito_client_service_set_property;
  object_class->dispose = mojito_client_service_dispose;
  object_class->finalize = mojito_client_service_finalize;

  signals[AVATAR_RETRIEVED_SIGNAL] =
    g_signal_new ("avatar-retrieved",
                  MOJITO_CLIENT_TYPE_SERVICE,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MojitoClientServiceClass, avatar_retrieved),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  signals[CAPS_CHANGED_SIGNAL] =
    g_signal_new ("capabilities-changed",
                  MOJITO_CLIENT_TYPE_SERVICE,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MojitoClientServiceClass, capabilities_changed),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRV);

  signals[STATUS_UPDATED_SIGNAL] =
    g_signal_new ("status-updated",
                  MOJITO_CLIENT_TYPE_SERVICE,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MojitoClientServiceClass, status_updated),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  signals[USER_CHANGED_SIGNAL] =
    g_signal_new ("user-changed",
                  MOJITO_CLIENT_TYPE_SERVICE,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (MojitoClientServiceClass, user_changed),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
mojito_client_service_init (MojitoClientService *self)
{
}

static void
_avatar_retrieved_cb (DBusGProxy *proxy,
                      char       *path,
                      gpointer    user_data)
{
  MojitoClientService *service = MOJITO_CLIENT_SERVICE (user_data);
  g_signal_emit (service, signals[AVATAR_RETRIEVED_SIGNAL], 0, path);
}

static void
_capabilities_changed_cb (DBusGProxy *proxy,
                          char      **caps,
                          gpointer    user_data)
{
  MojitoClientService *service = MOJITO_CLIENT_SERVICE (user_data);
  g_signal_emit (service, signals[CAPS_CHANGED_SIGNAL], 0, caps);
}

static void
_status_updated_cb (DBusGProxy *proxy,
                    gboolean    success,
                    gpointer    user_data)
{
  MojitoClientService *service = MOJITO_CLIENT_SERVICE (user_data);
  g_signal_emit (service, signals[STATUS_UPDATED_SIGNAL], 0, success);
}

static void
_user_changed_cb (DBusGProxy *proxy,
                          gpointer    user_data)
{
  MojitoClientService *service = MOJITO_CLIENT_SERVICE (user_data);
  g_signal_emit (service, signals[USER_CHANGED_SIGNAL], 0);
}

gboolean
_mojito_client_service_setup_proxy (MojitoClientService  *service,
                                    const gchar          *service_name,
                                    GError              **error_out)
{
  MojitoClientServicePrivate *priv = GET_PRIVATE (service);
  GError *error = NULL;
  gchar *path;

  priv->connection = dbus_g_bus_get (DBUS_BUS_STARTER, &error);

  if (!priv->connection)
  {
    g_critical (G_STRLOC ": Error getting DBUS connection: %s",
                error->message);
    g_propagate_error (error_out, error);
    return FALSE;
  }

  priv->name = g_strdup (service_name);

  path = g_strdup_printf (MOJITO_CLIENT_SERVICE_OBJECT, service_name);
  priv->proxy = dbus_g_proxy_new_for_name_owner (priv->connection,
                                                 MOJITO_CLIENT_SERVICE_NAME,
                                                 path,
                                                 MOJITO_CLIENT_SERVICE_INTERFACE,
                                                 &error);
  g_free (path);

  if (!priv->proxy)
  {
    g_critical (G_STRLOC ": Error setting up proxy for remote object: %s",
                error->message);
    g_propagate_error (error_out, error);
    return FALSE;
  }

  dbus_g_proxy_add_signal (priv->proxy,
                           "AvatarRetrieved",
                           G_TYPE_STRING,
                           G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (priv->proxy,
                               "AvatarRetrieved",
                               (GCallback)_avatar_retrieved_cb,
                               service,
                               NULL);

  dbus_g_proxy_add_signal (priv->proxy,
                           "CapabilitiesChanged",
                           G_TYPE_STRV,
                           NULL);
  dbus_g_proxy_connect_signal (priv->proxy,
                               "CapabilitiesChanged",
                               (GCallback)_capabilities_changed_cb,
                               service,
                               NULL);

  dbus_g_proxy_add_signal (priv->proxy,
                           "StatusUpdated",
                           G_TYPE_BOOLEAN,
                           G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (priv->proxy,
                               "StatusUpdated",
                               (GCallback)_status_updated_cb,
                               service,
                               NULL);

  dbus_g_proxy_add_signal (priv->proxy,
                           "UserChanged",
                           G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (priv->proxy,
                               "UserChanged",
                               (GCallback)_user_changed_cb,
                               service,
                               NULL);

  return TRUE;
}

/* Lets use the same closure structure for them all. Allocate using slicer */
typedef struct
{
  MojitoClientService *service;
  GCallback cb;
  gpointer userdata;
} MojitoClientServiceCallClosure;

static void
_get_capabilities_cb (DBusGProxy *proxy,
                      char      **caps,
                      GError     *error,
                      gpointer    userdata)
{
  MojitoClientServiceCallClosure *closure = (MojitoClientServiceCallClosure *)userdata;
  MojitoClientServiceGetCapabilitiesCallback cb;

  cb = (MojitoClientServiceGetCapabilitiesCallback)closure->cb;
  if (error)
  {
    g_warning (G_STRLOC ": Error getting capabilities: %s",
               error->message);
    cb (closure->service,
      NULL,
      error,
      closure->userdata);
    g_error_free (error);
  } else {
    cb (closure->service, (const char**)caps, error, closure->userdata);
    g_strfreev (caps);
  }

  g_object_unref (closure->service);
  g_slice_free (MojitoClientServiceCallClosure, closure);
}

void
mojito_client_service_get_static_capabilities (MojitoClientService                       *service,
                                               MojitoClientServiceGetCapabilitiesCallback cb,
                                               gpointer                                   userdata)
{
  MojitoClientServicePrivate *priv = GET_PRIVATE (service);
  MojitoClientServiceCallClosure *closure;

  closure = g_slice_new0 (MojitoClientServiceCallClosure);
  closure->service = g_object_ref (service);
  closure->cb = (GCallback)cb;
  closure->userdata = userdata;

  com_intel_Mojito_Service_get_static_capabilities_async (priv->proxy,
                                                          _get_capabilities_cb,
                                                          closure);
}

void
mojito_client_service_get_dynamic_capabilities (MojitoClientService                       *service,
                                                MojitoClientServiceGetCapabilitiesCallback cb,
                                                gpointer                                   userdata)
{
  MojitoClientServicePrivate *priv = GET_PRIVATE (service);
  MojitoClientServiceCallClosure *closure;

  closure = g_slice_new0 (MojitoClientServiceCallClosure);
  closure->service = g_object_ref (service);
  closure->cb = (GCallback)cb;
  closure->userdata = userdata;

  com_intel_Mojito_Service_get_dynamic_capabilities_async (priv->proxy,
                                                           _get_capabilities_cb,
                                                           closure);
}

static void
_update_status_cb (DBusGProxy *proxy,
                   GError     *error,
                   gpointer    userdata)
{
  MojitoClientServiceCallClosure *closure = (MojitoClientServiceCallClosure *)userdata;
  MojitoClientServiceUpdateStatusCallback cb;

  if (error)
  {
    g_warning (G_STRLOC ": Error updating status: %s",
               error->message);
  }

  cb = (MojitoClientServiceUpdateStatusCallback)closure->cb;
  cb (closure->service,
      error,
      closure->userdata);

  if (error)
    g_error_free (error);

  g_object_unref (closure->service);
  g_slice_free (MojitoClientServiceCallClosure, closure);
}



void
mojito_client_service_update_status (MojitoClientService                    *service,
                                     MojitoClientServiceUpdateStatusCallback cb,
                                     const gchar                            *status_msg,
                                     gpointer                                userdata)
{
  MojitoClientServicePrivate *priv = GET_PRIVATE (service);
  MojitoClientServiceCallClosure *closure;

  closure = g_slice_new0 (MojitoClientServiceCallClosure);
  closure->service = g_object_ref (service);
  closure->cb = (GCallback)cb;
  closure->userdata = userdata;

  com_intel_Mojito_Service_update_status_async (priv->proxy,
                                                status_msg,
                                                _update_status_cb,
                                                closure);
}

static void
_request_avatar_cb (DBusGProxy *proxy, GError *error, gpointer userdata)
{
  /* TODO: print the error to the console? */
}

void
mojito_client_service_request_avatar (MojitoClientService *service)
{
  MojitoClientServicePrivate *priv = GET_PRIVATE (service);

  com_intel_Mojito_Service_request_avatar_async (priv->proxy, _request_avatar_cb, NULL);
}

const char *
mojito_client_service_get_name (MojitoClientService *service)
{
  return GET_PRIVATE (service)->name;
}
