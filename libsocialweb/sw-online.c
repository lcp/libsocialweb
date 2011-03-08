/*
 * libsocialweb - social data store
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
#include "sw-online.h"
#include "sw-debug.h"
#include "sw-marshals.h"

/* This is the common infrastructure */
typedef struct {
  SwOnlineNotify callback;
  gpointer user_data;
} ListenerData;

static GList *listeners = NULL;

static gboolean online_init (void);

void
sw_online_add_notify (SwOnlineNotify callback,
                      gpointer       user_data)
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
sw_online_remove_notify (SwOnlineNotify callback,
                         gpointer       user_data)
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

static void
emit_notify (gboolean online)
{
  GList *l;

  SW_DEBUG (ONLINE, "Now %s", online ? "online" : "offline");

  for (l = listeners; l; l = l->next) {
    ListenerData *data = l->data;
    data->callback (online, data->user_data);
  }
}

#if WITH_ONLINE_ALWAYS

/*
 * A bit nasty but this case never uses emit_notify we get a compile warning
 * otherwise.
 */
static const gpointer dummy = &emit_notify;

static gboolean
online_init (void)
{
  return FALSE;
}

gboolean
sw_is_online (void)
{
  return TRUE;
}

#endif

#if WITH_ONLINE_NM
#include <libnm-glib/nm-client.h>

#if !defined(NM_CHECK_VERSION)
#define NM_CHECK_VERSION(x,y,z) 0
#endif

/*
 * Use NMClient since it correctly handles the NetworkManager service
 * appearing and disappearing, as can happen at boot time, or during
 * a network subsystem restart.
 */
static NMClient *client = NULL;

static gboolean
we_are_online (gpointer user_data)
{
  emit_notify (sw_is_online ());
  return FALSE;
}

static void
state_changed (NMClient         *client,
               const GParamSpec *pspec,
               gpointer          data)
{
  if (sw_is_online()) {
    /* NM is notifying us too early - workaround that */
    g_timeout_add (1500, (GSourceFunc)we_are_online, NULL);
  } else {
    emit_notify (FALSE); /* sw_is_online ()); */
  }
}

static gboolean
online_init (void)
{
  if (!client) {
    client = nm_client_new();
    g_signal_connect (client,
                      "notify::" NM_CLIENT_STATE,
		      G_CALLBACK (state_changed),
                      NULL);
  }
  return TRUE;
}

gboolean
sw_is_online (void)
{
  if (!online_init ())
    return TRUE;

  switch (nm_client_get_state (client)) {
#if NM_CHECK_VERSION(0,8,992)
  case NM_STATE_CONNECTED_LOCAL:
  case NM_STATE_CONNECTED_SITE:
  case NM_STATE_CONNECTED_GLOBAL:
#else
  case NM_STATE_CONNECTED:
#endif
    return TRUE;
  case NM_STATE_CONNECTING:
  case NM_STATE_ASLEEP:
  case NM_STATE_DISCONNECTED:
#if NM_CHECK_VERSION(0,8,992)
  case NM_STATE_DISCONNECTING:
#endif
  case NM_STATE_UNKNOWN:
  default:
    return FALSE;
  }
}

#endif


#if WITH_ONLINE_CONNMAN
#include <string.h>
#include <dbus/dbus-glib.h>

static DBusGProxy *proxy = NULL;
static gboolean current_state;

#define STRING_VARIANT_HASHTABLE (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))

static void
state_changed (DBusGProxy *proxy, const char *new_state)
{
  current_state = (g_strcmp0 (new_state, "online") == 0);
  emit_notify (current_state);
}

static void
got_state_cb (DBusGProxy     *proxy,
              DBusGProxyCall *call,
              void           *user_data)
{
  char *state = NULL;
  GError *error = NULL;

  if (!dbus_g_proxy_end_call (proxy, call, &error,
			      G_TYPE_STRING, &state,
			      G_TYPE_INVALID)) {
    g_printerr ("Cannot get current online state: %s", error->message);
    g_error_free (error);
    return;
  }

  current_state = (g_strcmp0 (state, "online") == 0);
  emit_notify (current_state);
  g_free (state);
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
                                     "net.connman",
                                     "/",
                                     "net.connman.Manager");

  dbus_g_object_register_marshaller (g_cclosure_marshal_VOID__STRING,
                                     G_TYPE_NONE, G_TYPE_STRING,
				     G_TYPE_INVALID);
  dbus_g_proxy_add_signal (proxy, "StateChanged", G_TYPE_STRING, NULL);
  dbus_g_proxy_connect_signal (proxy, "StateChanged",
                               (GCallback)state_changed, NULL, NULL);

  current_state = FALSE;

  /* Get the current state */
  dbus_g_proxy_begin_call (proxy, "GetState", got_state_cb,
                           NULL, NULL, G_TYPE_INVALID);

  return TRUE;
}


gboolean
sw_is_online (void)
{
  if (!online_init ())
    return TRUE;

  return current_state;
}
#endif

#if WITH_ONLINE_TEST
#include "sw-online-testui.c"
#endif
