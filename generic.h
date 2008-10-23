#include <sqlite3.h>
#include <glib.h>

#ifndef __GENERIC_H
#define __GENERIC_H

typedef enum {
  BIND_NULL,
  BIND_TEXT,
  BIND_INT,
  BIND_INT64,
} BindType;

void db_generic_bind (sqlite3_stmt *statement, ...);
sqlite3_stmt *db_generic_prepare_and_bind (sqlite3 *db, const char *query,  ...);
gboolean db_generic_fetch (sqlite3 *db, sqlite3_stmt *statement, gboolean dup, gboolean destroy, ...);
int db_generic_exec (sqlite3_stmt *statement, gboolean finalize);
int db_generic_reset_and_finalize (sqlite3_stmt *statement);

void db_begin_transaction (sqlite3 *db);
void db_commit_transaction (sqlite3 *db);
void db_rollback_transaction (sqlite3 *db);

#endif /* __GENERIC_H */
