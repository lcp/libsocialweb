#include <math.h>
#include "mojito-view.h"
#include "mojito-view-ginterface.h"

#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>

static void view_iface_init (gpointer g_iface, gpointer iface_data);
G_DEFINE_TYPE_WITH_CODE (MojitoView, mojito_view, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MOJITO_TYPE_VIEW_IFACE, view_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_VIEW, MojitoViewPrivate))

struct _MojitoViewPrivate {
  GList *sources;
  guint count;
  guint timeout_id;
  /* The set of pending sources we're waiting for updates from during an update cycle */
  MojitoSet *pending_sources;
  MojitoSet *pending_items;
  /* The items we've sent to the client */
  MojitoSet *current;
};

enum {
  PROP_0,
  PROP_COUNT,
};

static int
get_source_count (GHashTable *hash, MojitoSource *source)
{
  return GPOINTER_TO_INT (g_hash_table_lookup (hash, source));
}

static int
inc_source_count (GHashTable *hash, MojitoSource *source)
{
  int count;
  count = GPOINTER_TO_INT (g_hash_table_lookup (hash, source));
  ++count;
  g_hash_table_insert (hash, source, GINT_TO_POINTER (count));
}

static void
send_added (gpointer data, gpointer user_data)
{
  MojitoItem *item = data;
  MojitoView *view = user_data;

  mojito_view_iface_emit_item_added (view,
                                     "TODO source",
                                     mojito_item_get (item, "id"),
                                     0,
                                     mojito_item_peek_hash (item));
}

static void
send_removed (gpointer data, gpointer user_data)
{
  MojitoItem *item = data;
  MojitoView *view = user_data;

  mojito_view_iface_emit_item_removed (view, "TODO source", mojito_item_get (item, "id"));
}


static void
source_updated (MojitoSource *source, MojitoSet *set, gpointer user_data)
{
  MojitoView *view = MOJITO_VIEW (user_data);
  MojitoViewPrivate *priv = view->priv;
  GList *list;
  int count, source_max;
  GHashTable *counts;
  MojitoSet *new;

  mojito_set_remove (priv->pending_sources, (GObject*)source);

  mojito_set_add_from (priv->pending_items, set);
  mojito_set_unref (set);

  /* Are we still waiting for replies? */
  if (!mojito_set_is_empty (priv->pending_sources))
    return;

  /* The magic */
  list = mojito_set_as_list (priv->pending_items);
  mojito_set_empty (priv->pending_items);

  list = g_list_sort (list, (GCompareFunc)mojito_item_compare_date_newer);

  counts = g_hash_table_new (NULL, NULL);
  source_max = ceil ((float)priv->count / g_list_length (priv->sources));

  count = 0;
  new = mojito_item_set_new ();

  /* We manipulate list in place instead of using a temporary GList* because we
     manipulate the list so need to track the new tip */
  while (list && count <= priv->count) {
    MojitoItem *item;
    MojitoSource *source;

    item = list->data;
    source = mojito_item_get_source (item);

    if (get_source_count (counts, source) >= source_max) {
      list = list->next;
      continue;
    }

    /* New list steals the reference */
    mojito_set_add (new, (GObject*)item);
    g_object_unref (item);
    list = g_list_delete_link (list, list);

    inc_source_count (counts, source);
  }
  /* Rewind back to the beginning */
  list = g_list_first (list);

  /* If we still don't have enough items and there are spare, add them. */
  while (list && count <= priv->count) {
    GObject *item = list->data;
    mojito_set_add (new, item);
    g_object_unref (item);
    list = g_list_delete_link (list, list);
  }

  /* Clean up */
  while (list) {
    g_object_unref (list->data);
    list = g_list_delete_link (list, list);
  }

  /* update seen uids and emit signals */
  MojitoSet *old_items, *new_items;

  old_items = mojito_set_difference (priv->current, new);
  new_items = mojito_set_difference (new, priv->current);

  mojito_set_foreach (old_items, (GFunc)send_removed, view);
  mojito_set_foreach (new_items, (GFunc)send_added, view);

  mojito_set_unref (old_items);
  mojito_set_unref (new_items);

  mojito_set_unref (priv->current);
  priv->current = new;
}

static gboolean
start_update (MojitoView *view)
{
  MojitoViewPrivate *priv = view->priv;
  GList *l;

  mojito_set_empty (priv->pending_sources);
  mojito_set_empty (priv->pending_items);

  for (l = priv->sources; l; l = l->next) {
    MojitoSource *source = l->data;
    mojito_set_add (priv->pending_sources, g_object_ref (source));
  }

  for (l = priv->sources; l; l = l->next) {
    MojitoSource *source = l->data;
    mojito_source_update (source, source_updated, view);
  }
}

static void
view_start (MojitoViewIface *iface, DBusGMethodInvocation *context)
{
  MojitoView *view = MOJITO_VIEW (iface);
  MojitoViewPrivate *priv = view->priv;

  mojito_view_iface_return_from_start (context);

  if (priv->timeout_id == 0) {
    /* TODO: 60 seconds for testing, should be 15 minutes or so */
    priv->timeout_id = g_timeout_add_seconds (60, (GSourceFunc)start_update, iface);
    start_update (view);
  }
}

static void
mojito_view_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  MojitoViewPrivate *priv = MOJITO_VIEW (object)->priv;

  switch (property_id) {
  case PROP_COUNT:
    g_value_set_uint (value, priv->count);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_view_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  MojitoViewPrivate *priv = MOJITO_VIEW (object)->priv;

  switch (property_id) {
  case PROP_COUNT:
    priv->count = g_value_get_uint (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_view_dispose (GObject *object)
{
  MojitoViewPrivate *priv = MOJITO_VIEW (object)->priv;

  while (priv->sources) {
    g_object_unref (priv->sources->data);
    priv->sources = g_list_delete_link (priv->sources, priv->sources);
  }

  G_OBJECT_CLASS (mojito_view_parent_class)->dispose (object);
}

static void
view_iface_init (gpointer g_iface, gpointer iface_data)
{
  MojitoViewIfaceClass *klass = (MojitoViewIfaceClass*)g_iface;
  mojito_view_iface_implement_start (klass, view_start);
}

static void
mojito_view_class_init (MojitoViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MojitoViewPrivate));

  object_class->get_property = mojito_view_get_property;
  object_class->set_property = mojito_view_set_property;
  object_class->dispose = mojito_view_dispose;

  pspec = g_param_spec_uint ("count", "count", "The number of items",
                             1, G_MAXUINT, 10,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_COUNT, pspec);
}

static void
mojito_view_init (MojitoView *self)
{
  self->priv = GET_PRIVATE (self);

  self->priv->pending_sources = mojito_set_new ();
  self->priv->pending_items = mojito_item_set_new ();
  self->priv->current = mojito_item_set_new ();
}

MojitoView*
mojito_view_new (guint count)
{
  return g_object_new (MOJITO_TYPE_VIEW,
                       "count", count,
                       NULL);
}

static void
on_item_added (MojitoSource *source,
               const gchar  *uuid,
               gint64        date,
               GHashTable  *props,
               MojitoView  *view)
{
  g_assert (MOJITO_IS_SOURCE (source));
  g_assert (MOJITO_IS_VIEW (view));

  mojito_view_iface_emit_item_added (view,
                                     mojito_source_get_name (source),
                                     uuid,
                                     date,
                                     props);
}

void
mojito_view_add_source (MojitoView *view, MojitoSource *source)
{
  MojitoViewPrivate *priv = view->priv;

  priv->sources = g_list_append (priv->sources, g_object_ref (source));
}
