#include <glib-object.h>
#include <sqlite3.h>
#include <libsoup/soup.h>
#include "generic.h"

static SoupSession *session;

static const char sql_get_sources[] = "SELECT rowid, type, url, etag, modified FROM sources;";
static const char sql_update_source[] = "UPDATE sources SET etag=:etag, modified=:modified WHERE rowid=:rowid;";

typedef struct {
  sqlite3_int64 rowid;
  const char *type, *url, *etag;
  time_t last;
} UpdateData;

static gboolean
create_tables (sqlite3 *db)
{
  sqlite3_stmt *statement = NULL;
  const char *command, sql[] = "CREATE TABLE IF NOT EXISTS sources ("
    "'uid' INTEGER PRIMARY KEY,"
    "'type' TEXT NOT NULL," /* string identifier for the type */
    "'url' TEXT NOT NULL," /* URL */
    "'etag' TEXT," /* Last seen ETag */
    "'modified' INTEGER" /* Last modified time in UTC as seconds from 1970-1-1 */
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
update_source (UpdateData *data, sqlite3 *db)
{
  SoupMessage *msg;

  g_print ("url %s\n", data->url);
  
  msg = soup_message_new (SOUP_METHOD_GET, data->url);
  
  if (data->etag) {
    soup_message_headers_append (msg->request_headers, "If-None-Match", data->etag);
  }
  
  if (data->last)  {
    SoupDate *date;
    char *s;
    date = soup_date_new_from_time_t (data->last);
    s = soup_date_to_string (date, SOUP_DATE_HTTP);
    soup_message_headers_append (msg->request_headers, "If-Modified-Since", s);
    g_free (s);
  }
  
  switch (soup_session_send_message (session, msg)) {
    /* TODO: use SOUP_STATUS_IS_SUCCESSFUL() etc */
  case SOUP_STATUS_OK:
    {
      sqlite3_stmt *statement;
      const char *etag, *header;
      time_t modified = 0;
      
      etag = soup_message_headers_get (msg->response_headers, "ETag");
      
      header = soup_message_headers_get (msg->response_headers, "Last-Modified");
      if (header) {
        SoupDate *date;
        date = soup_date_new_from_string (header);
        modified = soup_date_to_time_t (date);
        soup_date_free (date);
      }
      
      g_print ("new data\n");
      
      statement = db_generic_prepare_and_bind (db, sql_update_source,
                                               ":rowid", BIND_INT64, data->rowid,
                                               ":etag", BIND_TEXT, etag,
                                               ":modified", BIND_INT, modified,
                                               NULL);
      if (db_generic_exec (statement, TRUE) != SQLITE_OK) {
        g_error ("Cannot update source: %s", sqlite3_errmsg (db));
      }
    }
    break;
  case SOUP_STATUS_NOT_MODIFIED:
    g_print ("nothing changed\n");
    break;
  default:
    g_print ("Unhandled response\n");
    break;
  }
  
  g_object_unref (msg);
}

static void
iterate_sources (sqlite3 *db)
{
  int res;
  sqlite3_stmt *statement;
  sqlite3_int64 rowid;
  GList *l = NULL;
  char *type, *url, *etag;
  time_t last;

  res = sqlite3_prepare_v2 (db, sql_get_sources, -1, &statement, NULL);
  if (res != SQLITE_OK) g_error (sqlite3_errmsg (db));

  while (db_generic_fetch (db, statement, TRUE, TRUE,
                           0, BIND_INT64, &rowid,
                           1, BIND_TEXT, &type,
                           2, BIND_TEXT, &url,
                           3, BIND_TEXT, &etag,
                           4, BIND_INT, &last,
                           -1)) {
    UpdateData *data;
    data = g_slice_new0 (UpdateData);
    data->rowid = rowid;
    data->type = type;
    data->url = url;
    data->etag = etag;
    data->last = last;

    l = g_list_prepend (l, data);
  }

  g_list_foreach (l, (GFunc)update_source, db);
}

int
main (int argc, char **argv)
{
  sqlite3 *db;

  g_assert (sqlite3_threadsafe ());
  
  g_thread_init (NULL);
  g_type_init ();

  if (sqlite3_open ("test.db", &db) != SQLITE_OK) {
    g_error (sqlite3_errmsg (db));
    return 1;
  }

  if (!create_tables (db)) {
    g_error (sqlite3_errmsg (db));
    return 1;
  }

  session = soup_session_sync_new ();

  iterate_sources (db);
  
  return 0;
}
