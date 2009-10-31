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
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <mojito/mojito-core.h>
#include <mojito/mojito-debug.h>
#include "poll.h"

static char *debug_opts = NULL;

static const GOptionEntry entries[] = {
  /* TODO: extract the debug flags and list them here */
  { "debug", 'd', 0, G_OPTION_ARG_STRING, &debug_opts, "Debug flags", "DEBUG" },
  { NULL }
};

static gboolean
request_name (void)
{
  DBusGConnection *connection;
  DBusGProxy *proxy;
  guint32 request_status;
  GError *error = NULL;

  connection = dbus_g_bus_get (DBUS_BUS_STARTER, &error);
  if (connection == NULL) {
    g_printerr ("Failed to open connection to DBus: %s\n", error->message);
    g_error_free (error);
    return FALSE;
  }

  proxy = dbus_g_proxy_new_for_name (connection,
                                     DBUS_SERVICE_DBUS,
                                     DBUS_PATH_DBUS,
                                     DBUS_INTERFACE_DBUS);

  if (!org_freedesktop_DBus_request_name (proxy, "com.intel.Mojito",
                                          DBUS_NAME_FLAG_DO_NOT_QUEUE, &request_status,
                                          &error)) {
    g_printerr ("Failed to request name: %s\n", error->message);
    g_error_free (error);
    return FALSE;
  }

  return request_status == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER;
}

int
main (int argc, char **argv)
{
  GOptionContext *context;
  GError *error = NULL;
  MojitoCore *core;

  g_thread_init (NULL);
  g_type_init ();
  g_set_prgname ("mojito");
  g_set_application_name ("Mojito");

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
  if (!g_option_context_parse (context, &argc, &argv, &error)) {
    g_printerr ("Cannot parse options: %s\n", error->message);
    return 1;
  }

  mojito_debug_init (debug_opts ? debug_opts : g_getenv ("MOJITO_DEBUG"));

  core = mojito_core_dup_singleton ();

  if (MOJITO_DEBUG_ENABLED (MAIN_LOOP))
    poll_init ();

  if (!request_name ())
    return 0;

  mojito_core_run (core);

  g_object_unref (core);

  return 0;
}
