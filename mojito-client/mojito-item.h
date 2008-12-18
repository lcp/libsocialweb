#include <glib.h>

typedef struct {
  volatile gint refcount;
  gchar *source;
  gchar *uuid;
  time_t date;
  GHashTable *props;
} MojitoItem;

void mojito_item_unref (MojitoItem *item);
void mojito_item_ref (MojitoItem *item);
void mojito_item_free (MojitoItem *item);
MojitoItem *mojito_item_new (void);

