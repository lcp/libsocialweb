#ifndef _MOJITO_ITEM
#define _MOJITO_ITEM

#include <glib-object.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_ITEM mojito_item_get_type()

#define MOJITO_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_ITEM, MojitoItem))

#define MOJITO_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_ITEM, MojitoItemClass))

#define MOJITO_IS_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_ITEM))

#define MOJITO_IS_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_ITEM))

#define MOJITO_ITEM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_ITEM, MojitoItemClass))

typedef struct _MojitoItemPrivate MojitoItemPrivate;

typedef struct {
  GObject parent;
  MojitoItemPrivate *priv;
} MojitoItem;

typedef struct {
  GObjectClass parent_class;
} MojitoItemClass;

GType mojito_item_get_type (void);

MojitoItem* mojito_item_new (void);

void mojito_item_put (MojitoItem *item, const char *key, const char *value);

void mojito_item_take (MojitoItem *item, const char *key, char *value);

const char * mojito_item_get (MojitoItem *item, const char *key);

int mojito_item_compare_date_older (MojitoItem *a, MojitoItem *b);

int mojito_item_compare_date_newer (MojitoItem *a, MojitoItem *b);

void mojito_item_dump (MojitoItem *item);

G_END_DECLS

#endif /* _MOJITO_ITEM */
