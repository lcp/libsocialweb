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

#include <config.h>

#include <glib.h>
#include "mojito-online.h"

#if WITH_ONLINE_ALWAYS
gboolean
mojito_is_online (void)
{
  return TRUE;
}
#endif

#if WITH_ONLINE_NM
#include <NetworkManager.h>
#include <dbus/dbus-glib.h>

/* TODO: this should create a persistant proxy and watch for signals instead of
   making a method call every time, to save traffic. */
gboolean
mojito_is_online (void)
{
  /* On error, assume we are online */
  GError *error = NULL;
  DBusGConnection *conn;
  DBusGProxy *proxy;
  NMState state = 0;

  conn = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
  if (!conn)
    return TRUE;

  proxy = dbus_g_proxy_new_for_name (conn, NM_DBUS_SERVICE,
                                     NM_DBUS_PATH,
                                     NM_DBUS_INTERFACE);

  dbus_g_proxy_call (proxy, "state", &error,
                     G_TYPE_INVALID,
                     G_TYPE_UINT, &state, G_TYPE_INVALID);

  g_object_unref (proxy);

  return state == NM_STATE_CONNECTED;
}
#endif
