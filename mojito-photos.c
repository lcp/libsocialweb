#include "mojito-photos.h"
#include "mojito-utils.h"
#include "generic.h"

static const char sql_create[] = 
  "CREATE TABLE IF NOT EXISTS photos ("
  "'source' TEXT NOT NULL," /* source id */
  "'uuid' TEXT NOT NULL," /* item id */
  /* TODO: time_t or iso8901 string? */
  "'date' INTEGER NOT NULL," /* post date */
  "'link' TEXT NOT NULL," /* photo link, should point to a HTML page */
  "'title' TEXT" /* photo title */
  /* TODO: local thumbnail path? embed thumbnail into db? */
  ");";
static const char sql_add_item[] = "INSERT INTO "
  "photos(source, uuid, date, link, title) "
  "VALUES (:source, :uuid, :date, :link, :title);";
static const char sql_delete_items[] = "DELETE FROM photos WHERE source=:source;";

void
mojito_photos_initialize (MojitoCore *core)
{
  sqlite3 *db;
  
  db = mojito_core_get_db (core);
  
  if (!mojito_create_tables (db, sql_create)) {
    g_printerr ("Cannot create tables for photos: %s\n", sqlite3_errmsg (db));
  }
}

void
mojito_photos_add (MojitoCore *core, const char *source_id, const char *item_id, time_t date, const char *link, const char *title)
{
  sqlite3 *db;
  sqlite3_stmt *statement;
  
  g_return_if_fail (MOJITO_IS_CORE (core));
  g_return_if_fail (source_id);
  g_return_if_fail (item_id);
  
  db = mojito_core_get_db (core);
  statement = db_generic_prepare_and_bind (db, sql_add_item,
                                           ":source", BIND_TEXT, source_id,
                                           ":uuid", BIND_TEXT, item_id,
                                           ":date", BIND_INT, date,
                                           ":link", BIND_TEXT, link,
                                           ":title", BIND_TEXT, title,
                                           NULL);
  if (db_generic_exec (statement, TRUE) != SQLITE_OK) {
    g_printerr ("cannot add item\b");
  }
}


void
mojito_photos_remove (MojitoCore *core, const char *source_id)
{
  sqlite3_stmt *statement;
  sqlite3 *db;
  
  g_return_if_fail (MOJITO_IS_CORE (core));
  g_return_if_fail (source_id);

  db = mojito_core_get_db (core);
  statement = db_generic_prepare_and_bind (db, sql_delete_items,
                                           ":source", BIND_TEXT, source_id, NULL);
  if (db_generic_exec (statement, TRUE) != SQLITE_OK) {
    g_printerr ("cannot remove items\n");
  }
}
