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
#include "mojito-online.h"
#include "mojito-view.h"
#include "mojito-banned.h"
#include "client-monitor.h"

#include "mojito-core-ginterface.h"


static void core_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (MojitoCore, mojito_core, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MOJITO_TYPE_CORE_IFACE,
                                                core_iface_init));

#define GET_PRIVATE(o)                                                  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_CORE, MojitoCorePrivate))

struct _MojitoCorePrivate {
  DBusGConnection *connection;
  /* Hash of service name to GType */
  GHashTable *service_types;
  /* Hash of service name to MojitoServiceProxies on the bus */
  GHashTable *bus_services;
  /* Hash of service name-parameter hash to MojitoService instance. */
  GHashTable *active_services;
  /* Hash of banned UID strings to a senitel (TODO: to timestamp) */
  GHashTable *banned_uids;
  /* List of open views */
  GList *views;
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

  l = g_hash_table_get_keys (priv->service_types);
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

static GHashTable *
make_param_hash (const char *s)
{
  char **tokens, **i;
  GHashTable *hash;

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if (s) {
    tokens = g_strsplit (s, ",", 0);
    for (i = tokens; *i; i++) {
      char **nv;
      nv = g_strsplit (*i, "=", 2);
      if (nv[0] && nv[1]) {
        /* Hash takes ownership of the strings */
        g_hash_table_insert (hash, nv[0], nv[1]);
        /* Just free the array */
        g_free (nv);
      } else {
        g_strfreev (nv);
      }
    }
    g_strfreev (tokens);
  }

  return hash;
}

typedef struct {
  MojitoCore *core;
  char *key;
} WeakServiceData;

static void
service_destroy_notify (gpointer user_data, GObject *dead_service)
{
  WeakServiceData *data = user_data;

  /* We need to do steal and free because the object is already dead */
  g_hash_table_steal (data->core->priv->active_services, data->key);
  g_free (data->key);
  g_slice_free (WeakServiceData, data);
}

/* For the given name and parameters, return reference to a MojitoService. */
static MojitoService *
get_service (MojitoCore *core, const char *name, GHashTable *params)
{
  MojitoCorePrivate *priv = core->priv;
  char *param_hash, *key;
  MojitoService *service;
  GType type;
  WeakServiceData *data;

  param_hash = mojito_hash_string_dict (params);
  key = g_strconcat (name, "-", param_hash, NULL);
  g_free (param_hash);

  service = g_hash_table_lookup (priv->active_services, key);
  if (service) {
    g_free (key);
    return g_object_ref (service);
  }

  type = GPOINTER_TO_INT (g_hash_table_lookup (priv->service_types, name));
  if (!type)
    return NULL;

  service = g_object_new (type, "params", params, NULL);
  g_hash_table_insert (priv->active_services, key, service);

  data = g_slice_new0 (WeakServiceData);
  data->core = core;
  data->key = key;
  g_object_weak_ref ((GObject*)service, service_destroy_notify, data);

  return service;
}

static void
view_weak_notify_list (gpointer data, GObject *old_view)
{
  MojitoCore *core = data;

  g_assert (MOJITO_IS_CORE (core));

  core->priv->views = g_list_remove (core->priv->views, old_view);
}

static void
open_view (MojitoCoreIface *self, const char **services, guint count, DBusGMethodInvocation *context)
{
  MojitoCore *core = MOJITO_CORE (self);
  MojitoCorePrivate *priv = core->priv;
  MojitoView *view;
  char *path;
  const char **i;

  view = mojito_view_new (core, count);
  path = make_path ();
  dbus_g_connection_register_g_object (priv->connection, path, (GObject*)view);

  for (i = services; *i; i++) {
    char **tokens;
    const char *name;
    GHashTable *params;
    MojitoService *service;

    tokens = g_strsplit (*i, ":", 2);
    name = tokens[0];
    params = make_param_hash (tokens[1]);

    g_message ("%s: service name %s", __FUNCTION__, name);

    service = get_service (core, name, params);

    if (service) {
      mojito_view_add_service (view, service, params);
    } else {
      g_warning (G_STRLOC ": Request for unknown service: %s",
                 name);

      g_strfreev (tokens);
    }

    g_hash_table_unref (params);
  }

  /* TODO: move this into the view? */
  client_monitor_add (dbus_g_method_get_sender (context), (GObject*)view);
  g_object_weak_ref ((GObject*)view, view_weak_notify, dbus_g_method_get_sender (context));

  g_object_weak_ref ((GObject*)view, view_weak_notify_list, core);
  priv->views = g_list_prepend (priv->views, view);

  mojito_core_iface_return_from_open_view (context, path);

  g_free (path);
}

static void
core_hide_item (MojitoCoreIface *iface, const gchar *uid, DBusGMethodInvocation *context)
{
  MojitoCore *core = MOJITO_CORE (iface);

  g_hash_table_insert (core->priv->banned_uids, g_strdup (uid), GINT_TO_POINTER (42));

  g_list_foreach (core->priv->views, (GFunc)mojito_view_recalculate, NULL);

  /* TODO: do in an idle or on quit? */
  mojito_ban_save (core->priv->banned_uids);

  mojito_core_iface_return_from_hide_item (context);
}

/* Online notifications */
static void
is_online (MojitoCoreIface *self, DBusGMethodInvocation *context)
{
  dbus_g_method_return (context, mojito_is_online ());
}

static void
online_changed (gboolean online, gpointer user_data)
{
  MojitoCore *core = MOJITO_CORE (user_data);

  mojito_core_iface_emit_online_changed (core, online);
}

static void
load_module (MojitoCore *core, const char *file)
{
  MojitoCorePrivate *priv = core->priv;
  GModule *service_module;
  const gchar *service_name;
  GType service_type;
  gpointer sym;
  MojitoServiceProxy *proxy;
  gchar *path;

  service_module = g_module_open (file, G_MODULE_BIND_LOCAL);
  if (service_module == NULL) {
    g_critical (G_STRLOC ": error opening module: %s",
                g_module_error());
    g_module_close (service_module);
    return;
  }

  if (!g_module_symbol (service_module, "mojito_module_get_name", &sym)) {
    g_critical ("Cannot get symbol mojito_module_get_name: %s",
                g_module_error());
    g_module_close (service_module);
    return;
  } else {
    service_name = (*(MojitoModuleGetNameFunc)sym)();
  }

  if (!g_module_symbol (service_module, "mojito_module_get_type", &sym)) {
    g_critical ("Cannot get symbol mojito_module_get_type: %s",
                g_module_error());
    g_module_close (service_module);
    return;
  } else {
    service_type = (*(MojitoModuleGetTypeFunc)sym)();
  }

  if (service_name && service_type) {
    g_module_make_resident (service_module);

    /* Add to the service name -> type hash */
    g_hash_table_insert (priv->service_types,
                         (char*)service_name,
                         GINT_TO_POINTER (service_type));

    /* Create an instance and add it to the bus */
    proxy = g_object_new (service_type, NULL);
    g_hash_table_insert (priv->bus_services,
                         (gchar *)service_name,
                         proxy);
    path = g_strdup_printf ("/com/intel/Mojito/Service/%s", service_name);
    dbus_g_connection_register_g_object (priv->connection,
                                         path,
                                         (GObject*)proxy);
    g_free (path);
    g_message ("Imported module: %s", service_name);
  }
}

static void
load_modules_from_dir (MojitoCore *core)
{
  GFile *services_dir_file;
  GFileEnumerator *enumerator;
  GError *error = NULL;
  GFileInfo *fi;

  services_dir_file = g_file_new_for_path (MOJITO_SERVICES_MODULES_DIR);

  enumerator = g_file_enumerate_children (services_dir_file,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          &error);
  g_object_unref (services_dir_file);

  if (!enumerator)
  {
    g_critical (G_STRLOC ": error whilst enumerating directory children: %s",
                error->message);
    g_clear_error (&error);
    return;
  }

  while ((fi = g_file_enumerator_next_file (enumerator, NULL, &error)) != NULL)
  {
    if (g_str_has_suffix (g_file_info_get_name (fi), ".so"))
    {
      char *module_path;

      module_path = g_build_filename (MOJITO_SERVICES_MODULES_DIR,
                                      g_file_info_get_name (fi),
                                      NULL);
      load_module (core, module_path);
      g_free (module_path);
    }
  }

  if (error)
  {
    g_critical (G_STRLOC ": error whilst enumerating directory children: %s",
                error->message);
    g_clear_error (&error);
  }

  g_object_unref (enumerator);
}

static void
load_modules_from_string (MojitoCore *core, const char *s)
{
  char **modules, **basename;

  modules = g_strsplit (s, ",", 0);

  for (basename = modules; *basename; basename++) {
    char *name, *path;

    name = g_strconcat ("lib", *basename, NULL);
    path = g_build_filename (MOJITO_SERVICES_MODULES_DIR, name, NULL);

    load_module (core, path);

    g_free (path);
    g_free (name);
  }

  g_strfreev (modules);
}

static void
mojito_core_constructed (GObject *object)
{
  MojitoCore *core = MOJITO_CORE (object);
  MojitoCorePrivate *priv = core->priv;
  GError *error = NULL;
  const char *modules;

  priv->banned_uids = mojito_ban_load ();

  priv->connection = dbus_g_bus_get (DBUS_BUS_STARTER, &error);
  if (error) {
    g_warning ("Failed to open connection to DBus: %s\n", error->message);
    g_error_free (error);
  }

  dbus_g_connection_register_g_object (priv->connection, "/com/intel/Mojito", object);

  client_monitor_init (priv->connection);

  modules = g_getenv ("MOJITO_MODULES");
  if (modules) {
    load_modules_from_string (core, modules);
  } else {
    load_modules_from_dir (core);
  }

  mojito_online_add_notify (online_changed, object);
}

static void
mojito_core_dispose (GObject *object)
{
  G_OBJECT_CLASS (mojito_core_parent_class)->dispose (object);
}

static void
mojito_core_finalize (GObject *object)
{
  mojito_online_remove_notify (online_changed, object);

  G_OBJECT_CLASS (mojito_core_parent_class)->finalize (object);
}

static void
core_iface_init (gpointer g_iface, gpointer iface_data)
{
  MojitoCoreIfaceClass *klass = (MojitoCoreIfaceClass*)g_iface;

  mojito_core_iface_implement_get_services (klass, get_services);
  mojito_core_iface_implement_open_view (klass, open_view);
  mojito_core_iface_implement_hide_item (klass, core_hide_item);
  mojito_core_iface_implement_is_online (klass, is_online);
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

  priv->service_types = g_hash_table_new (g_str_hash, g_str_equal);

  priv->bus_services = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              NULL, g_object_unref);

  priv->active_services = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              g_free, g_object_unref);
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

gboolean
mojito_core_is_item_banned (MojitoCore *core, MojitoItem *item)
{
  const char *id;

  g_return_val_if_fail (MOJITO_IS_CORE (core), FALSE);
  g_return_val_if_fail (MOJITO_IS_ITEM (item), FALSE);

  id = mojito_item_get (item, "id");

  return g_hash_table_lookup (core->priv->banned_uids, id) != NULL;
}

MojitoCore *
mojito_core_dup_singleton (void)
{
  static MojitoCore *core = NULL;

  if (!core)
  {
    core = mojito_core_new ();
    g_object_add_weak_pointer (G_OBJECT (core), (gpointer)&core);
  }

  return core;
}
