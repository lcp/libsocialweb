#include <mojito/mojito-source.h>
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>

#include "lastfm.h"

G_DEFINE_TYPE (MojitoSourceLastfm, mojito_source_lastfm, MOJITO_TYPE_SOURCE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SOURCE_LASTFM, MojitoSourceLastfmPrivate))

struct _MojitoSourceLastfmPrivate {
  RestProxy *proxy;
};

static char *
get_name (MojitoSource *source)
{
  return "lastfm";
}

static void
mojito_source_lastfm_dispose (GObject *object)
{
  MojitoSourceLastfmPrivate *priv = ((MojitoSourceLastfm*)object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (mojito_source_lastfm_parent_class)->dispose (object);
}

static void
mojito_source_lastfm_finalize (GObject *object)
{
  G_OBJECT_CLASS (mojito_source_lastfm_parent_class)->finalize (object);
}

static void
mojito_source_lastfm_class_init (MojitoSourceLastfmClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoSourceClass *source_class = MOJITO_SOURCE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoSourceLastfmPrivate));

  object_class->dispose = mojito_source_lastfm_dispose;
  object_class->finalize = mojito_source_lastfm_finalize;

  source_class->get_name = get_name;
  //source_class->start = start;
}

static void
mojito_source_lastfm_init (MojitoSourceLastfm *self)
{
  self->priv = GET_PRIVATE (self);

  self->priv->proxy = rest_proxy_new ("http://ws.audioscrobbler.com/2.0/", FALSE);
}
