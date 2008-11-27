#include "mojito-view.h"
#include "mojito-view-ginterface.h"

static void view_iface_init (gpointer g_iface, gpointer iface_data);
G_DEFINE_TYPE_WITH_CODE (MojitoView, mojito_view, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MOJITO_TYPE_VIEW_IFACE, view_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_VIEW, MojitoViewPrivate))

struct _MojitoViewPrivate {
  GList *sources;
};

static void
view_start (MojitoViewIface *iface, DBusGMethodInvocation *context)
{
  MojitoViewPrivate *priv = MOJITO_VIEW (iface)->priv;
  GList *l;
  
  mojito_view_iface_return_from_start (context);

  for (l = priv->sources; l; l = l->next) {
    MojitoSource *source = l->data;
    mojito_source_start (source);
  }
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
  g_type_class_add_private (klass, sizeof (MojitoViewPrivate));
}

static void
mojito_view_init (MojitoView *self)
{
  self->priv = GET_PRIVATE (self);
}

MojitoView*
mojito_view_new (void)
{
  return g_object_new (MOJITO_TYPE_VIEW, NULL);
}

static void
on_item_added (MojitoSource *source, GHashTable *hash, MojitoView *view)
{
  g_assert (MOJITO_IS_SOURCE (source));
  g_assert (MOJITO_IS_VIEW (view));

  mojito_view_iface_emit_item_added (view, mojito_source_get_name (source), hash);
}

void
mojito_view_add_source (MojitoView *view, MojitoSource *source)
{
  MojitoViewPrivate *priv = view->priv;

  /* TODO: ref the sources and unref in dispose */
  
  g_debug ("adding source to view");
  priv->sources = g_list_append (priv->sources, source);
  
  g_signal_connect (source, "item-added", G_CALLBACK (on_item_added), view);
}
