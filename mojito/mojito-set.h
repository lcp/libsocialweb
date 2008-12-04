#ifndef _MOJITO_SET
#define _MOJITO_SET

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _MojitoSet MojitoSet;

#define MOJITO_TYPE_SET mojito_set_get_type ()

GType mojito_set_get_type (void);

MojitoSet *mojito_set_new (GHashFunc hash_func, GEqualFunc equal_func, GDestroyNotify destroy_func);

MojitoSet * mojito_set_ref (MojitoSet *set);

void mojito_set_unref (MojitoSet *set);

void mojito_set_add (MojitoSet *set, gpointer item);

void mojito_set_remove (MojitoSet *set, gpointer item);

gboolean mojito_set_has (MojitoSet *set, gpointer item);

gboolean mojito_set_is_empty (MojitoSet *set);

void mojito_set_empty (MojitoSet *set);

G_END_DECLS

#endif /* _MOJITO_SET */
