#include <string.h>
#include <glib-object.h>
#include <sqlite3.h>
#include <libsoup/soup.h>
#include "generic.h"
#include <libxml/xmlreader.h>

static SoupSession *session;

static const char sql_get_sources[] = "SELECT rowid, type, url, etag, modified FROM sources;";
static const char sql_update_source[] = "UPDATE sources SET etag=:etag, modified=:modified WHERE rowid=:rowid;";
static const char sql_add_blog[] = "INSERT INTO blogs(source, link, date, title) VALUES (:source, :link, :date, :title);";
static const char sql_delete_blogs[] = "DELETE FROM blogs WHERE source=:source;";

typedef struct {
  sqlite3_int64 rowid;
  const char *type, *url, *etag;
  time_t last;
} UpdateData;

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
    
    "CREATE TABLE IF NOT EXISTS blogs ("
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

/* libxml, i hate you */
#define C(s) ((char*)s)
#define X(s) ((xmlChar*)s)

static gboolean
is_element (xmlTextReaderPtr reader, const char *namespace, const char *name)
{
  return xmlTextReaderNodeType (reader) == XML_READER_TYPE_ELEMENT &&
    strcmp (C(xmlTextReaderConstNamespaceUri (reader)), namespace) == 0 &&
    strcmp (C(xmlTextReaderConstLocalName (reader)), name) == 0;
}

static gboolean
is_end_element (xmlTextReaderPtr reader, const char *namespace, const char *name)
{
  return xmlTextReaderNodeType (reader) == XML_READER_TYPE_END_ELEMENT &&
    strcmp (C(xmlTextReaderConstNamespaceUri (reader)), namespace) == 0 &&
    strcmp (C(xmlTextReaderConstLocalName (reader)), name) == 0;
}

static time_t
time_t_from_string (const char *s)
{
  SoupDate *date;
  time_t t;
  date = soup_date_new_from_string (s);
  t = soup_date_to_time_t (date);
  soup_date_free (date);
  return t;
}

#define NS_ATOM "http://www.w3.org/2005/Atom"
static void
update_blog (sqlite3 *db, UpdateData *data, SoupMessage *msg)
{
  xmlTextReaderPtr reader;
  sqlite3_stmt *statement;

  statement = db_generic_prepare_and_bind (db, sql_delete_blogs,
                                           ":source", BIND_INT64, data->rowid,
                                           NULL);
  if (db_generic_exec (statement, TRUE) != SQLITE_OK) {
    g_error ("cannot remove blogs");
  }

  reader = xmlReaderForMemory (msg->response_body->data,
                               msg->response_body->length,
                               /* TODO: pass URL */ NULL,
                               NULL, 0);

  if (!xmlTextReaderRead(reader))
    return;
  
  if (is_element (reader, NS_ATOM, "feed")) {
    while (xmlTextReaderRead (reader) == 1) {
      if (is_element (reader, NS_ATOM, "entry")) {
        char *title = NULL, *link = NULL;
        time_t date = 0;
        int depth;

        depth = xmlTextReaderDepth (reader);
        while (xmlTextReaderRead (reader) == 1 && ! is_end_element (reader, NS_ATOM, "entry")) {
          /* We only care about direct child nodes */
          if (xmlTextReaderDepth (reader) != depth + 1)
            continue;

          if (is_element (reader, NS_ATOM, "link")) {
            const char *rel = NULL;
            if (xmlTextReaderMoveToAttribute (reader, X("rel")) == 1) {
              rel = C(xmlTextReaderConstValue (reader));
            }
            if ((rel == NULL || strcmp (rel, "alternate") == 0) && 
                xmlTextReaderMoveToAttribute (reader, X("href")) == 1) {
              link = C(xmlTextReaderValue (reader));
            }
          }

          if (is_element (reader, NS_ATOM, "title")) {
            title = C(xmlTextReaderReadString (reader));
          }
          if (is_element (reader, NS_ATOM, "updated")) {
            char *s;
            s = C(xmlTextReaderReadString (reader));
            date = time_t_from_string (s);
            xmlFree (s);
          }
        }

        if (link) {
          sqlite3_stmt *statement;
          statement = db_generic_prepare_and_bind (db, sql_add_blog,
                                                   ":source", BIND_INT64, data->rowid,
                                                   ":link", BIND_TEXT, link,
                                                   ":date", BIND_INT, date,
                                                   ":title", BIND_TEXT, title,
                                                   NULL);
          if (db_generic_exec (statement, TRUE) != SQLITE_OK) {
            g_error ("cannot add blog");
          }
          g_free (link);
          g_free (title);
        }
      }
    }
  }
}

static void
update_flickr (sqlite3 *db, UpdateData *data, SoupMessage *msg)
{
}

static void
update_source (UpdateData *data, sqlite3 *db)
{
  SoupMessage *msg;

  g_print ("url %s\n", data->url);
  
  msg = soup_message_new (SOUP_METHOD_GET, data->url);
  
#ifndef NO_CACHE
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
#endif

  switch (soup_session_send_message (session, msg)) {
    /* TODO: use SOUP_STATUS_IS_SUCCESSFUL() etc */
  case SOUP_STATUS_OK:
    {
      sqlite3_stmt *statement;
      const char *etag, *header;
      time_t modified = 0;
      
      etag = soup_message_headers_get (msg->response_headers, "ETag");
      
      header = soup_message_headers_get (msg->response_headers, "Last-Modified");
      if (header)
        modified = time_t_from_string (header);
      
      g_print ("new data\n");
      
      if (strcmp (data->type, "blog") == 0) {
        update_blog (db, data, msg);
      } else if (strcmp (data->type, "flickr") == 0) {
        update_flickr (db, data, msg);
      } else {
        g_debug ("Unknown feed type '%s'", data->type);
      }
      
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
