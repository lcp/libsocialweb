#include <glib.h>

typedef struct {
  gchar *source;
  gchar *uuid;
  time_t issue_time;
  time_t expire_time;
  GHashTable *props;
} MojitoItem;

MojitoItem *mojito_item_new (void);
void mojito_item_free (MojitoItem *item);
void mojito_item_set_prop (MojitoItem *item, const gchar *prop, const gchar *value);

