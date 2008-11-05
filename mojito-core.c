#include <sqlite3.h>
#include <libsoup/soup.h>
#include "mojito-core.h"
#include "generic.h"

#include "mojito-source-blog.h"

G_DEFINE_TYPE (MojitoCore, mojito_core, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_CORE, MojitoCorePrivate))

struct _MojitoCorePrivate {
  /* Hash of interned source identifiers to GTypes */
  GHashTable *source_hash;
  GList *sources;
  sqlite3 *db;
  SoupSession *session;
};

static gboolean
create_tables (sqlite3 *db)
{
  sqlite3_stmt *statement = NULL;
  const char *command, sql[] = 
    "CREATE TABLE IF NOT EXISTS sources ("
    "'uid' INTEGER PRIMARY KEY,"
    "'type' TEXT NOT NULL," /* string identifier for the type */
    "'url' TEXT NOT NULL," /* URL */
    "'etag' TEXT," /* Last seen ETag */
    "'modified' INTEGER" /* Last modified time in UTC as seconds from 1970-1-1 */
    ");"
    
    "CREATE TABLE IF NOT EXISTS items ("
    "'rowid' INTEGER PRIMARY KEY,"
    "'source' INTEGER NOT NULL,"
    "'link' TEXT NOT NULL,"
    "'date' INTEGER NOT NULL,"
    "'title' TEXT"
    ");";
  command = sql;
  
  do {
    if (sqlite3_prepare_v2 (db, command, -1, &statement, &command)) {
      g_warning ("Cannot create tables (prepare): %s", sqlite3_errmsg (db));
      sqlite3_finalize (statement);
      return FALSE;
    }
    
    if (statement && db_generic_exec (statement, TRUE) != SQLITE_OK) {
      g_warning ("Cannot create tables (step): %s", sqlite3_errmsg (db));
      return FALSE;
    }
  } while (statement);

  return TRUE;
}


static void
mojito_core_dispose (GObject *object)
{
  MojitoCorePrivate *priv = MOJITO_CORE (object)->priv;

  if (priv->session) {
    g_object_unref (priv->session);
    priv->session = NULL;
  }
  
  while (priv->sources) {
    g_object_unref (priv->sources->data);
    priv->sources = g_list_delete_link (priv->sources, priv->sources);
  }
  
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
mojito_core_class_init (MojitoCoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoCorePrivate));

  object_class->dispose = mojito_core_dispose;
  object_class->finalize = mojito_core_finalize;
}

static const char sql_get_sources[] = "SELECT rowid, type, url FROM sources;";

static void
populate_sources (MojitoCore *core)
{
  MojitoCorePrivate *priv = core->priv;
  sqlite3_stmt *statement;
  sqlite3_int64 rowid = 0;
  const char *type = NULL, *url = NULL;

  if (sqlite3_prepare_v2 (priv->db, sql_get_sources, -1, &statement, NULL) != SQLITE_OK) {
    g_warning (sqlite3_errmsg (priv->db));
    return;
  }
  
  while (db_generic_fetch (priv->db, statement, FALSE, TRUE,
                           0, BIND_INT64, &rowid,
                           1, BIND_TEXT, &type,
                           2, BIND_TEXT, &url,
                           -1)) {
    GType gtype;
    MojitoSource *source;

    gtype = GPOINTER_TO_INT
      (g_hash_table_lookup (core->priv->source_hash, g_intern_string (type)));
    
    if (gtype == 0)
      continue;
    
    source = g_object_new (gtype, NULL);
  }
}

static void
mojito_core_init (MojitoCore *self)
{
  MojitoCorePrivate *priv = GET_PRIVATE (self);
  self->priv = priv;

  g_assert (sqlite3_threadsafe ());
  
  priv->source_hash = g_hash_table_new (g_direct_hash, g_direct_equal);

  priv->sources = NULL;

  /* Make async when we use DBus etc */
  priv->session = soup_session_sync_new ();

  /* TODO: move here onwards into a separate function so we can return errors */
  
  /* TODO: load these at runtime */
  g_hash_table_insert (priv->source_hash,
                       (gpointer)g_intern_static_string ("blog"),
                       GINT_TO_POINTER (MOJITO_TYPE_SOURCE_BLOG));
  
  if (sqlite3_open ("test.db", &priv->db) != SQLITE_OK) {
    g_error (sqlite3_errmsg (priv->db));
    return;
  }

  if (!create_tables (priv->db)) {
    g_error (sqlite3_errmsg (priv->db));
    return;
  }

  populate_sources (self);
}

MojitoCore*
mojito_core_new (void)
{
  return g_object_new (MOJITO_TYPE_CORE, NULL);
}

void
mojito_core_run (MojitoCore *core)
{
  // iterate_sources
}
