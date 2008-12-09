#include <sqlite3.h>
#include <libsoup/soup.h>
#include "mojito-core.h"
#include "mojito-utils.h"
#include "mojito-view.h"

#include "mojito-core-ginterface.h"

static void core_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (MojitoCore, mojito_core, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MOJITO_TYPE_CORE_IFACE, 
                                                core_iface_init));

#define GET_PRIVATE(o)                                                  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_CORE, MojitoCorePrivate))

struct _MojitoCorePrivate {
  DBusGConnection *connection;
  /* Hash of source name to GTypes */
  GHashTable *source_names;
  /* Hash of source names to instances */
  GHashTable *sources;
  
  sqlite3 *db;
  SoupSession *session;
};

typedef const gchar *(*MojitoModuleGetNameFunc)(void);
typedef const GType (*MojitoModuleGetTypeFunc)(void);

static void
get_sources (MojitoCoreIface *self, DBusGMethodInvocation *context)
{
  MojitoCore *core = MOJITO_CORE (self);
  MojitoCorePrivate *priv = core->priv;
  GPtrArray *array;
  GList *l;

  array = g_ptr_array_new ();

  l = g_hash_table_get_keys (priv->source_names);
  while (l) {
    g_ptr_array_add (array, l->data);
    l = g_list_delete_link (l, l);
  }
  g_ptr_array_add (array, NULL);

  mojito_core_iface_return_from_get_sources (context, (const gpointer)array->pdata);

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
open_view (MojitoCoreIface *self, const char **sources, guint count, DBusGMethodInvocation *context)
{
  MojitoCore *core = MOJITO_CORE (self);
  MojitoCorePrivate *priv = core->priv;
  MojitoView *view;
  char *path;
  const char **i;

  view = mojito_view_new ();
  path = make_path ();
  dbus_g_connection_register_g_object (priv->connection, path, (GObject*)view);

  for (i = sources; *i; i++) {
    const char *name = *i;
    MojitoSource *source;
    GType source_type;

    g_debug ("%s: source name %s", __FUNCTION__, name);

    source = g_hash_table_lookup (priv->sources, name);

    if (source == NULL) {
      source_type = GPOINTER_TO_INT
        (g_hash_table_lookup (priv->source_names, name));
      
      if (source_type) {
        source = g_object_new (source_type,
                               "core", core,
                               NULL);
        
        /* TODO: make this a weak reference so the entry can be removed when the
           last view closes */
        g_hash_table_insert (priv->sources, g_strdup (name), g_object_ref (source));
      }
    }
    
    if (source) {
      mojito_view_add_source (view, source);
    }
  }
  
  mojito_core_iface_return_from_open_view (context, path);

  g_free (path);
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
}

static void
mojito_core_dispose (GObject *object)
{
  MojitoCorePrivate *priv = MOJITO_CORE (object)->priv;

  if (priv->session) {
    g_object_unref (priv->session);
    priv->session = NULL;
  }
  
  /* TODO: free source_names */
  
  G_OBJECT_CLASS (mojito_core_parent_class)->dispose (object);
}

static void
mojito_core_finalize (GObject *object)
{
  MojitoCorePrivate *priv = MOJITO_CORE (object)->priv;

  sqlite3_close (priv->db);
  
  G_OBJECT_CLASS (mojito_core_parent_class)->finalize (object);
}

static void
core_iface_init (gpointer g_iface, gpointer iface_data)
{
  MojitoCoreIfaceClass *klass = (MojitoCoreIfaceClass*)g_iface;

  mojito_core_iface_implement_get_sources (klass, get_sources);
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
populate_sources (MojitoCore *core)
{
  /* FIXME: Get the sources from directory */
  MojitoCorePrivate *priv = core->priv;
  GFile *sources_dir_file;
  GFileEnumerator *enumerator;
  GError *error = NULL;
  GFileInfo *fi;
  gchar *module_path = NULL;
  GModule *source_module;
  const gchar *source_name;
  GType source_type;
  gpointer sym;
  MojitoSource *source;

  sources_dir_file = g_file_new_for_path (MOJITO_SOURCES_MODULES_DIR);

  enumerator = g_file_enumerate_children (sources_dir_file, 
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          &error);

  if (!enumerator)
  {
    g_critical (G_STRLOC ": error whilst enumerating directory children: %s",
                error->message);
    g_clear_error (&error);
    g_object_unref (sources_dir_file);
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

    module_path = g_build_filename (MOJITO_SOURCES_MODULES_DIR,
                                    g_file_info_get_name (fi),
                                    NULL);
    source_module = g_module_open (module_path, 
                                   G_MODULE_BIND_LOCAL | G_MODULE_BIND_LAZY);
    g_module_make_resident (source_module);

    source_name = NULL;
    if (!g_module_symbol (source_module, "mojito_module_get_name", &sym))
    {
      g_critical (G_STRLOC ": error getting symbol: %s",
                  g_module_error());
    } else {
      source_name = (*(MojitoModuleGetNameFunc)sym)();
    }

    source_type = 0;
    if (!g_module_symbol (source_module, "mojito_module_get_type", &sym))
    {
      g_critical (G_STRLOC ": error getting symbol: %s",
                  g_module_error());
    } else {
      source_type =  (*(MojitoModuleGetTypeFunc)sym)();
    }

    if (source_name && source_type)
    {
      g_hash_table_insert (priv->source_names, (gchar *)source_name, GINT_TO_POINTER (source_type));
      g_debug (G_STRLOC ": Imported module: %s", source_name);
    }

    fi = g_file_enumerator_next_file (enumerator, NULL, &error);
  }

  if (error)
  {
    g_critical (G_STRLOC ": error whilst enumerating directory children: %s",
                error->message);
    g_clear_error (&error);
  }

  g_object_unref (sources_dir_file);
  g_object_unref (enumerator);
}

static void
mojito_core_init (MojitoCore *self)
{
  MojitoCorePrivate *priv = GET_PRIVATE (self);
  char *db_path;

  self->priv = priv;

  g_assert (sqlite3_threadsafe ());
  
  /* TODO: check free policy */
  priv->source_names = g_hash_table_new (g_str_hash, g_str_equal);
  priv->sources = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  /* Make async when we use DBus etc */
  priv->session = soup_session_sync_new ();

  /* TODO: move here onwards into a separate function so we can return errors */
  
  db_path = g_build_filename (g_get_user_cache_dir (), "mojito.db", NULL);
  if (sqlite3_open (db_path, &priv->db) != SQLITE_OK) {
    g_free (db_path);
    g_error (sqlite3_errmsg (priv->db));
    return;
  }
  g_free (db_path);
  
  populate_sources (self);
}

MojitoCore*
mojito_core_new (void)
{
  return g_object_new (MOJITO_TYPE_CORE, NULL);
}

sqlite3 *
mojito_core_get_db (MojitoCore *core)
{
  g_return_val_if_fail (MOJITO_IS_CORE (core), NULL);

  return core->priv->db;
}

SoupSession *
mojito_core_get_session (MojitoCore *core)
{
  g_return_val_if_fail (MOJITO_IS_CORE (core), NULL);

  return core->priv->session;
}

void
mojito_core_run (MojitoCore *core)
{
  g_return_if_fail (MOJITO_IS_CORE (core));

  //g_list_foreach (core->priv->sources, (GFunc)mojito_source_update, NULL);

  GMainLoop *loop;
  loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);
}
