#ifndef _MOJITO_SET
#define _MOJITO_SET

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _MojitoSet MojitoSet;

#define MOJITO_TYPE_SET mojito_set_get_type ()

GType mojito_set_get_type (void);

MojitoSet *mojito_set_new (void);

MojitoSet * mojito_set_new_full (GHashFunc hash_func, GEqualFunc equal_func);

MojitoSet * mojito_set_ref (MojitoSet *set);

void mojito_set_unref (MojitoSet *set);

void mojito_set_add (MojitoSet *set, GObject *item);

void mojito_set_remove (MojitoSet *set, GObject *item);

gboolean mojito_set_has (MojitoSet *set, GObject *item);

gboolean mojito_set_is_empty (MojitoSet *set);

void mojito_set_empty (MojitoSet *set);

MojitoSet * mojito_set_union (MojitoSet *set_a, MojitoSet *set_b);

MojitoSet * mojito_set_difference (MojitoSet *set_a, MojitoSet *set_b);

void mojito_set_add_from (MojitoSet *set, MojitoSet *from);

GList * mojito_set_as_list (MojitoSet *set);

MojitoSet * mojito_set_from_list (GList *list);

void mojito_set_foreach (MojitoSet *set, GFunc func, gpointer user_data);

G_END_DECLS

#endif /* _MOJITO_SET */
