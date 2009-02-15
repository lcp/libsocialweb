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

#include <gio/gio.h>
#include <dbus/dbus-glib-lowlevel.h>
#include "mojito-core.h"
#include "mojito-utils.h"
#include "mojito-view.h"
#include "client-monitor.h"
#include "mojito-service-proxy.h"

#include "mojito-core-ginterface.h"


static void core_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (MojitoCore, mojito_core, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MOJITO_TYPE_CORE_IFACE,
                                                core_iface_init));

#define GET_PRIVATE(o)                                                  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_CORE, MojitoCorePrivate))

struct _MojitoCorePrivate {
  DBusGConnection *connection;
  /* Hash of service name to MojitoServiceProxy (but we can just pretend they
   * are services. */
  GHashTable *available_services;
};

typedef const gchar *(*MojitoModuleGetNameFunc)(void);
typedef const GType (*MojitoModuleGetTypeFunc)(void);

static void
get_services (MojitoCoreIface *self, DBusGMethodInvocation *context)
{
  MojitoCore *core = MOJITO_CORE (self);
  MojitoCorePrivate *priv = core->priv;
  GPtrArray *array;
  GList *l;

  array = g_ptr_array_new ();

  l = g_hash_table_get_keys (priv->available_services);
  while (l) {
    g_ptr_array_add (array, l->data);
    l = g_list_delete_link (l, l);
  }
  g_ptr_array_add (array, NULL);

  mojito_core_iface_return_from_get_services (context, (const gpointer)array->pdata);

  g_ptr_array_free (array, TRUE);
}

static char *
make_path (void)
{
  static volatile int counter = 1;
  return g_strdup_printf ("/com/intel/Mojito/View/%d",
                          g_atomic_int_exchange_and_add (&counter, 1));
}

static void
view_weak_notify (gpointer data, GObject *old_view)
{
  char *sender = data;

  g_assert (data);

  client_monitor_remove (sender, old_view);
}

static void
open_view (MojitoCoreIface *self, const char **services, guint count, DBusGMethodInvocation *context)
{
  MojitoCore *core = MOJITO_CORE (self);
  MojitoCorePrivate *priv = core->priv;
  MojitoView *view;
  char *path;
  const char **i;

  view = mojito_view_new (count);
  path = make_path ();
  dbus_g_connection_register_g_object (priv->connection, path, (GObject*)view);

  for (i = services; *i; i++) {
    const char *name = *i;
    MojitoService *service;

    g_debug ("%s: service name %s", __FUNCTION__, name);

    service = g_hash_table_lookup (priv->available_services, name);

    if (service) {
      mojito_view_add_service (view, service);
    } else {
      g_warning (G_STRLOC ": Request for unknown service: %s",
                 name);
    }
  }

  client_monitor_add (dbus_g_method_get_sender (context), (GObject*)view);
  g_object_weak_ref ((GObject*)view, view_weak_notify, dbus_g_method_get_sender (context));

  mojito_core_iface_return_from_open_view (context, path);

  g_free (path);
}

static void
populate_services (MojitoCore *core)
{
  /* FIXME: Get the services from directory */
  MojitoCorePrivate *priv = core->priv;
  GFile *services_dir_file;
  GFileEnumerator *enumerator;
  GError *error = NULL;
  GFileInfo *fi;
  gchar *module_path = NULL;
  GModule *service_module;
  const gchar *service_name;
  GType service_type;
  gpointer sym;
  MojitoServiceProxy *proxy;

  services_dir_file = g_file_new_for_path (MOJITO_SERVICES_MODULES_DIR);

  enumerator = g_file_enumerate_children (services_dir_file,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          &error);

  if (!enumerator)
  {
    g_critical (G_STRLOC ": error whilst enumerating directory children: %s",
                error->message);
    g_clear_error (&error);
    g_object_unref (services_dir_file);
    return;
  }

  fi = g_file_enumerator_next_file (enumerator, NULL, &error);

  while (fi)
  {
    if (!(g_str_has_suffix (g_file_info_get_name (fi), ".so")))
    {
      fi = g_file_enumerator_next_file (enumerator, NULL, &error);
      continue;
    }

    module_path = g_build_filename (MOJITO_SERVICES_MODULES_DIR,
                                    g_file_info_get_name (fi),
                                    NULL);
    service_module = g_module_open (module_path, G_MODULE_BIND_LOCAL);
    if (service_module == NULL)
    {
      g_critical (G_STRLOC ": error opening module: %s",
                  g_module_error());
      continue;
    }

    service_name = NULL;
    if (!g_module_symbol (service_module, "mojito_module_get_name", &sym))
    {
      g_critical (G_STRLOC ": error getting symbol: %s",
                  g_module_error());
    } else {
      service_name = (*(MojitoModuleGetNameFunc)sym)();
    }

    service_type = 0;
    if (!g_module_symbol (service_module, "mojito_module_get_type", &sym))
    {
      g_critical (G_STRLOC ": error getting symbol: %s",
                  g_module_error());
    } else {
      service_type =  (*(MojitoModuleGetTypeFunc)sym)();
    }

    if (service_name && service_type)
    {
      proxy = mojito_service_proxy_new (core, service_type);
      g_hash_table_insert (priv->available_services, 
                           (gchar *)service_name, 
                           proxy);
      g_debug (G_STRLOC ": Imported module: %s", service_name);
    }

    g_module_make_resident (service_module);

    fi = g_file_enumerator_next_file (enumerator, NULL, &error);
  }

  if (error)
  {
    g_critical (G_STRLOC ": error whilst enumerating directory children: %s",
                error->message);
    g_clear_error (&error);
  }

  g_object_unref (services_dir_file);
  g_object_unref (enumerator);
}

static void
mojito_core_constructed (GObject *object)
{
  MojitoCorePrivate *priv = MOJITO_CORE (object)->priv;
  GError *error = NULL;

  priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (error) {
    g_warning ("Failed to open connection to DBus: %s\n", error->message);
    g_error_free (error);
  }

  dbus_g_connection_register_g_object (priv->connection, "/com/intel/Mojito", object);

  client_monitor_init (priv->connection);

  populate_services ((MojitoCore *)object);
}

static void
mojito_core_dispose (GObject *object)
{
  G_OBJECT_CLASS (mojito_core_parent_class)->dispose (object);
}

static void
mojito_core_finalize (GObject *object)
{
  G_OBJECT_CLASS (mojito_core_parent_class)->finalize (object);
}

static void
core_iface_init (gpointer g_iface, gpointer iface_data)
{
  MojitoCoreIfaceClass *klass = (MojitoCoreIfaceClass*)g_iface;

  mojito_core_iface_implement_get_services (klass, get_services);
  mojito_core_iface_implement_open_view (klass, open_view);
}

static void
mojito_core_class_init (MojitoCoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoCorePrivate));

  object_class->constructed = mojito_core_constructed;
  object_class->dispose = mojito_core_dispose;
  object_class->finalize = mojito_core_finalize;
}

static void
mojito_core_init (MojitoCore *self)
{
  MojitoCorePrivate *priv = GET_PRIVATE (self);

  self->priv = priv;

  /* TODO: check free policy */
  priv->available_services = g_hash_table_new (g_str_hash, g_str_equal);
}

MojitoCore*
mojito_core_new (void)
{
  return g_object_new (MOJITO_TYPE_CORE, NULL);
}

void
mojito_core_run (MojitoCore *core)
{
  GMainLoop *loop;

  g_return_if_fail (MOJITO_IS_CORE (core));

  loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);
}
