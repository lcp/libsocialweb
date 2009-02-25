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

G_DEFINE_TYPE (MojitoClientService, mojito_client_service, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_CLIENT_TYPE_SERVICE, MojitoClientServicePrivate))

typedef struct _MojitoClientServicePrivate MojitoClientServicePrivate;

struct _MojitoClientServicePrivate {
  DBusGConnection *connection;
  DBusGProxy *proxy;
};

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
}

static void
mojito_client_service_init (MojitoClientService *self)
{
}

gboolean
_mojito_client_service_setup_proxy (MojitoClientService  *service,
                                    const gchar          *service_name,
                                    GError              **error_out)
{
  MojitoClientServicePrivate *priv = GET_PRIVATE (service);
  GError *error = NULL;
  gchar *path;

  priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  if (!priv->connection)
  {
    g_critical (G_STRLOC ": Error getting DBUS connection: %s",
                error->message);
    g_propagate_error (error_out, error);
    return FALSE;
  }

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
                      gboolean    can_get_last_item,
                      gboolean    can_get_persona_icon,
                      gboolean    can_update_status,
                      GError     *error,
                      gpointer    userdata)
{
  MojitoClientServiceCallClosure *closure = (MojitoClientServiceCallClosure *)userdata;
  guint32 caps = 0;
  MojitoClientServiceGetCapabilitiesCallback cb;

  cb = (MojitoClientServiceGetCapabilitiesCallback)closure->cb;
  if (error)
  {
    cb (closure->service,
      0,
      error,
      closure->userdata);
  } else {
    if (can_get_last_item)
      caps |= MOJITO_CLIENT_SERVICE_CAN_GET_LAST_ITEM;
    if (can_get_persona_icon)
      caps |= MOJITO_CLIENT_SERVICE_CAN_GET_PERSONA_ICON;
    if (can_update_status)
      caps |= MOJITO_CLIENT_SERVICE_CAN_UPDATE_STATUS;
    cb (closure->service,
      caps,
      error,
      closure->userdata);
  }

  g_object_unref (closure->service);
  g_slice_free (MojitoClientServiceCallClosure, closure);
}

void
mojito_client_service_get_capabilities (MojitoClientService                       *service,
                                        MojitoClientServiceGetCapabilitiesCallback cb,
                                        gpointer                                   userdata)
{
  MojitoClientServicePrivate *priv = GET_PRIVATE (service);
  MojitoClientServiceCallClosure *closure;

  closure = g_slice_new0 (MojitoClientServiceCallClosure);
  closure->service = g_object_ref (service);
  closure->cb = (GCallback)cb;
  closure->userdata = userdata;

  com_intel_Mojito_Service_get_capabilities_async (priv->proxy,
                                                   _get_capabilities_cb,
                                                   closure);
}

static void
_get_last_item_cb (DBusGProxy *proxy,
                   GHashTable *hash,
                   GError     *error,
                   gpointer    userdata)
{
  MojitoClientServiceCallClosure *closure = (MojitoClientServiceCallClosure *)userdata;
  MojitoClientServiceGetLastItemCallback cb;
  MojitoItem *item = NULL;

  cb = (MojitoClientServiceGetLastItemCallback)closure->cb;
  if (error)
  {
    cb (closure->service,
        NULL,
        error,
        closure->userdata);
  } else {
    item = mojito_item_new ();
    /* TODO
       item->service = g_strdup (service);
       item->uuid = g_strdup (uuid);
       item->date.tv_sec = date;
    */
    item->props = g_hash_table_ref (hash);

    cb (closure->service,
        item,
        error,
        closure->userdata);

    mojito_item_unref (item);
  }

  g_object_unref (closure->service);
  g_slice_free (MojitoClientServiceCallClosure, closure);
}

void
mojito_client_service_get_last_item (MojitoClientService                     *service,
                                       MojitoClientServiceGetLastItemCallback cb,
                                       gpointer                                 userdata)
{
  MojitoClientServicePrivate *priv = GET_PRIVATE (service);
  MojitoClientServiceCallClosure *closure;

  closure = g_slice_new0 (MojitoClientServiceCallClosure);
  closure->service = g_object_ref (service);
  closure->cb = (GCallback)cb;
  closure->userdata = userdata;

  com_intel_Mojito_Service_get_last_item_async (priv->proxy,
                                                _get_last_item_cb,
                                                closure);
}

static void
_get_persona_icon_cb (DBusGProxy *proxy,
                      gchar      *persona_icon,
                      GError     *error,
                      gpointer    userdata)
{
  MojitoClientServiceCallClosure *closure = (MojitoClientServiceCallClosure *)userdata;
  MojitoClientServiceGetPersonaIconCallback cb;

  cb = (MojitoClientServiceGetPersonaIconCallback)closure->cb;

  if (error)
  {
    cb (closure->service,
        NULL,
        error,
        closure->userdata);
  } else {
    cb (closure->service,
        persona_icon,
        error,
        closure->userdata);
    g_free (persona_icon);
  }

  g_object_unref (closure->service);
  g_slice_free (MojitoClientServiceCallClosure, closure);
}

void
mojito_client_service_get_persona_icon (MojitoClientService                      *service,
                                        MojitoClientServiceGetPersonaIconCallback cb,
                                        gpointer                                  userdata)
{
  MojitoClientServicePrivate *priv = GET_PRIVATE (service);
  MojitoClientServiceCallClosure *closure;

  closure = g_slice_new0 (MojitoClientServiceCallClosure);
  closure->service = g_object_ref (service);
  closure->cb = (GCallback)cb;
  closure->userdata = userdata;

  com_intel_Mojito_Service_get_persona_icon_async (priv->proxy,
                                                   _get_persona_icon_cb,
                                                   closure);
}

static void
_update_status_cb (DBusGProxy *proxy,
                   gboolean    success,
                   GError     *error,
                   gpointer    userdata)
{
  MojitoClientServiceCallClosure *closure = (MojitoClientServiceCallClosure *)userdata;
  MojitoClientServiceUpdateStatusCallback cb;

  cb = (MojitoClientServiceUpdateStatusCallback)closure->cb;
  cb (closure->service,
      success,
      error,
      closure->userdata);

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

