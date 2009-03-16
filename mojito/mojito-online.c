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

/* This is the common infrastructure */
typedef struct {
  MojitoOnlineNotify callback;
  gpointer user_data;
} ListenerData;

static GList *listeners = NULL;

static gboolean online_init (void);

void
mojito_online_add_notify (MojitoOnlineNotify callback, gpointer user_data)
{
  ListenerData *data;

  if (!online_init ())
    return;

  data = g_slice_new (ListenerData);
  data->callback = callback;
  data->user_data = user_data;

  listeners = g_list_prepend (listeners, data);
}

void
mojito_online_remove_notify (MojitoOnlineNotify callback, gpointer user_data)
{
  GList *l = listeners;

  while (l) {
    ListenerData *data = l->data;
    if (data->callback == callback && data->user_data == user_data) {
      GList *next = l->next;
      listeners = g_list_delete_link (listeners, l);
      l = next;
    } else {
      l = l->next;
    }
  }
}


#if WITH_ONLINE_ALWAYS

static gboolean
online_init (void)
{
  return FALSE;
}

gboolean
mojito_is_online (void)
{
  return TRUE;
}

#endif

#if WITH_ONLINE_NM
#include <NetworkManager.h>
#include <dbus/dbus-glib.h>

static DBusGProxy *proxy = NULL;

static void
state_changed (DBusGProxy *proxy, NMState state, gpointer user_data)
{
  gboolean online = (state == NM_STATE_CONNECTED);
  GList *l;

  for (l = listeners; l; l = l->next) {
    ListenerData *data = l->data;
    data->callback (online, data->user_data);
  }
}

static gboolean
online_init (void)
{
  DBusGConnection *conn;

  if (proxy)
    return TRUE;

  conn = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
  if (!conn) {
    g_warning ("Cannot get connection to system message bus");
    return FALSE;
  }

  proxy = dbus_g_proxy_new_for_name (conn,
                                     NM_DBUS_SERVICE,
                                     NM_DBUS_PATH,
                                     NM_DBUS_INTERFACE);

  dbus_g_proxy_add_signal (proxy, "StateChanged", G_TYPE_UINT, NULL);
  dbus_g_proxy_connect_signal (proxy, "StateChanged",
                               (GCallback)state_changed, NULL, NULL);
  return TRUE;
}

gboolean
mojito_is_online (void)
{
  /* On error, assume we are online */
  NMState state = NM_STATE_CONNECTED;

  if (!online_init ())
    return TRUE;

  dbus_g_proxy_call (proxy, "state", NULL,
                     G_TYPE_INVALID,
                     G_TYPE_UINT, &state, G_TYPE_INVALID);

  return state == NM_STATE_CONNECTED;
}

#endif
