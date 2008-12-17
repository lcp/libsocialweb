#include <libsoup/soup.h>
#include "mojito-web.h"
#include "mojito-sqlite.h"

/* TODO: just store the date as the string? */
static const char sql_create_cache[] = "CREATE TABLE IF NOT EXISTS webcache ("
  "'url' TEXT PRIMARY KEY NOT NULL, 'etag' TEXT, 'modified' TEXT);";
static const char sql_get_cache[] = "SELECT etag, modified FROM webcache WHERE url=:url;";
/* GAR.  INSERT OR REPLACE doesn't do the right thing because it creates a new
   row instead of editing the existing one. */
static const char sql_insert_cache[] = "INSERT INTO webcache(url, etag, modified) VALUES(:url, :etag, :modified);";
static const char sql_update_cache[] = "UPDATE webcache SET etag=:etag, modified=:modified WHERE url=:url;";

guint
mojito_web_cached_send (MojitoCore *core, SoupSession *session, SoupMessage *msg)
{
  static gboolean created_tables = FALSE;
  sqlite3 *db = mojito_core_get_db (core);
  char *url;
  guint res;
#ifndef NO_CACHE
  sqlite3_stmt *statement;
  const char *etag = NULL, *last = NULL;
  gboolean update_cache = FALSE;
#endif

  if (G_UNLIKELY (!created_tables)) {
    /* TODO: not thread safe, use GOnce */
    if (!mojito_sqlite_create_tables (db, sql_create_cache))
      return SOUP_STATUS_IO_ERROR;
    created_tables = TRUE;
  }

  url = soup_uri_to_string (soup_message_get_uri (msg), FALSE);

#ifndef NO_CACHE
  statement = db_generic_prepare_and_bind (db, sql_get_cache, ":url", BIND_TEXT, url, NULL);
  if (db_generic_fetch (db, statement, FALSE, TRUE,
                        0, BIND_TEXT, &etag,
                        1, BIND_TEXT, &last,
                        -1)) {
    if (etag)
      soup_message_headers_append (msg->request_headers, "If-None-Match", etag);

    if (last)
      soup_message_headers_append (msg->request_headers, "If-Modified-Since", last);

    db_generic_reset_and_finalize (statement);
    update_cache = TRUE;
  }
#endif

  res = soup_session_send_message (session, msg);
#ifndef NO_CACHE
  if (res == SOUP_STATUS_OK) {
    /* update the cache */
    etag = soup_message_headers_get (msg->response_headers, "ETag");
    last = soup_message_headers_get (msg->response_headers, "Last-Modified");

    statement = db_generic_prepare_and_bind (db,
                                             update_cache ? sql_update_cache : sql_insert_cache,
                                             ":url", BIND_TEXT, url,
                                             ":etag", BIND_TEXT, etag,
                                             ":modified", BIND_TEXT, last,
                                             NULL);
    if (db_generic_exec (statement, TRUE) != SQLITE_OK) {
      g_error ("Cannot update source: %s", sqlite3_errmsg (db));
    }
  }
#endif

  g_free (url);

  return res;
}

char *
mojito_web_download_image (SoupSession *session, const char *url)
{
  char *md5, *path, *filename;

  g_return_val_if_fail (SOUP_IS_SESSION (session), NULL);
  g_return_val_if_fail (url, NULL);

  md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);

  /* TODO: pull this out into a separate file, twitter will need it too */
  path = g_build_filename (g_get_user_cache_dir (),
                           "mojito",
                           "thumbnails",
                           NULL);
  g_mkdir_with_parents (path, 0777);

  /* TODO: try and guess the extension from @url and use that */
  filename = g_build_filename (path, md5, NULL);
  g_free (md5);
  g_free (path);

  if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
    SoupMessage *msg;

    msg = soup_message_new (SOUP_METHOD_GET, url);
    soup_session_send_message (session, msg);
    if (msg->status_code = SOUP_STATUS_OK) {
      /* TODO: GError */
      g_file_set_contents (filename, msg->response_body->data, msg->response_body->length, NULL);
    } else {
      g_debug ("Cannot download %s: %s", url, msg->reason_phrase);
      g_free (filename);
      filename = NULL;
    }
    g_object_unref (msg);
  }

  return filename;
}
