#include <sqlite3.h>
#include <glib.h>
#include "mojito-sqlite.h"

/* column, type, location, ... */
gboolean
db_generic_fetch (sqlite3 *db, sqlite3_stmt *statement, gboolean dup, gboolean destroy, ...)
{
  va_list args;
  int res;
  int column;

  if (statement == NULL)
    return FALSE;

 retry:
  if ((res = sqlite3_step (statement)) == SQLITE_ROW) {
    va_start (args, destroy);
    column = va_arg (args, int);
    while (column != -1) {
      const BindType type = va_arg (args, BindType);

      switch (type) {
      case BIND_TEXT:
        {
          char **data = va_arg (args, char **);
          if (dup)
            *data = g_strdup ((char*)(sqlite3_column_text (statement, column)));
          else
            *data = (char*)(sqlite3_column_text (statement, column));
        }
        break;
      case BIND_INT:
        {
          int *data = va_arg (args, int*);
          *data = (sqlite3_column_int (statement, column));
        }
        break;
      case BIND_INT64:
        {
          sqlite3_int64 *data = va_arg (args, sqlite3_int64*);
          *data = (sqlite3_column_int64 (statement, column));
        }
        break;
      case BIND_NULL:
        /* Nothing to do */
        break;
      }
      column = va_arg (args, int);
    }
    va_end (args);
    return TRUE;
  } else {
    if (res == SQLITE_BUSY) {
      /* Deal with the possible concurrency from another process */
      g_usleep (G_USEC_PER_SEC / 100);
      goto retry;
    }
    if (res != SQLITE_DONE) {
      g_warning ("Error when retrieving row: %s", sqlite3_errmsg (db));
    }
    if (destroy)
      db_generic_reset_and_finalize (statement);
    else
      sqlite3_reset (statement);
    return FALSE;
  }
}


void
db_generic_bind_va (sqlite3_stmt *statement, va_list args)
{
  /* TODO: retval */
  int res;
  const gchar *id;

  g_return_if_fail (statement != NULL);

  while ((id = va_arg (args, char *)) != NULL) {
    const int index = sqlite3_bind_parameter_index (statement, id);
    const BindType type = va_arg (args, BindType);

    switch (type) {
    case BIND_NULL:
      res = sqlite3_bind_null (statement, index);
      break;
    case BIND_TEXT:
      res = sqlite3_bind_text (statement, index,
                               va_arg (args, char *), -1,
                               SQLITE_TRANSIENT);
      break;
    case BIND_INT:
      res = sqlite3_bind_int (statement, index, va_arg (args, int));
      break;
    case BIND_INT64:
      res = sqlite3_bind_int64 (statement, index, va_arg (args, sqlite3_int64));
      break;
    }

    if (res != SQLITE_OK) {
      g_warning ("Problem whilst binding statement: %s", sqlite3_errmsg (sqlite3_db_handle (statement)));
      sqlite3_finalize (statement);
      return;
    }
  }
}

/* NULL terminated list of parameter, type, value tuples */
void
db_generic_bind (sqlite3_stmt *statement, ...)
{
  /* TODO: retval */
  va_list args;

  va_start (args, statement);

  db_generic_bind_va (statement, args);

  va_end (args);
}

sqlite3_stmt *
db_generic_prepare_and_bind (sqlite3 *db, const gchar *query,  ...)
{
  va_list args;
  int res;
  const gchar *tail = NULL;
  sqlite3_stmt *statement = NULL;

  g_return_val_if_fail (db != NULL, NULL);
  g_return_val_if_fail (query != NULL, NULL);

  res = sqlite3_prepare_v2 (db, query, -1, &statement, &tail);

  if (res != SQLITE_OK) {
    g_warning ("Problem preparing sql statement: %s", sqlite3_errmsg (db));
    return NULL;
  }

  va_start (args, query);
  db_generic_bind_va (statement, args);
  va_end (args);

  return statement;
}

/* If finalize is TRUE, always reset and finalize.  If FALSE it resets if the
   step succeeded, and reset/finalizes if it errored. Returns SQLITE_OK on success*/
int
db_generic_exec (sqlite3_stmt *statement, gboolean finalize)
{
  sqlite3 *db = sqlite3_db_handle (statement);
  int res;

 retry:

  res = sqlite3_step (statement);

  if (res != SQLITE_DONE) {
    if (res == SQLITE_BUSY) {
      g_usleep (100);
      goto retry;
    }

    g_warning ("Cannot executing statement: %s", sqlite3_errmsg (db));
    return db_generic_reset_and_finalize (statement);
  }

  if (finalize)
    return db_generic_reset_and_finalize (statement);
  else
    return sqlite3_reset (statement);
}

int
db_generic_reset_and_finalize (sqlite3_stmt *statement)
{
  sqlite3 *db = sqlite3_db_handle (statement);
  int res;

  res = sqlite3_reset (statement);

  if (res != SQLITE_OK) {
    g_warning ("Problem resetting statement: %s", sqlite3_errmsg (db));
  }

  (void) sqlite3_finalize (statement);

  return res;
}

gboolean
mojito_sqlite_create_tables (sqlite3 *db, const char *sql)
{
  sqlite3_stmt *statement = NULL;
  const char *command = sql;

  do {
    if (sqlite3_prepare_v2 (db, command, -1, &statement, &command)) {
      g_printerr ("Cannot create tables (prepare): %s\n", sqlite3_errmsg (db));
      sqlite3_finalize (statement);
      return FALSE;
    }

    if (statement && db_generic_exec (statement, TRUE) != SQLITE_OK) {
      g_printerr ("Cannot create tables (step): %s\n", sqlite3_errmsg (db));
      return FALSE;
    }
  } while (statement);

  return TRUE;
}
