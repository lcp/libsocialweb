#include <stdlib.h>
#include "twitter.h"
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>
#include <mojito/mojito-utils.h>

G_DEFINE_TYPE (MojitoSourceTwitter, mojito_source_twitter, MOJITO_TYPE_SOURCE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SOURCE_TWITTER, MojitoSourceTwitterPrivate))

struct _MojitoSourceTwitterPrivate {
  char *user_id;
};

static void
update (MojitoSource *source, MojitoSourceDataFunc callback, gpointer user_data)
{
  //MojitoSourceTwitter *twitter = (MojitoSourceTwitter*)source;
}

static char *
mojito_source_twitter_get_name (MojitoSource *source)
{
  return "twitter";
}

static void
mojito_source_twitter_dispose (GObject *object)
{
  //MojitoSourceTwitterPrivate *priv = MOJITO_SOURCE_TWITTER (object)->priv;

  G_OBJECT_CLASS (mojito_source_twitter_parent_class)->dispose (object);
}

static void
mojito_source_twitter_finalize (GObject *object)
{
  MojitoSourceTwitterPrivate *priv = MOJITO_SOURCE_TWITTER (object)->priv;

  g_free (priv->user_id);

  G_OBJECT_CLASS (mojito_source_twitter_parent_class)->finalize (object);
}

static void
mojito_source_twitter_class_init (MojitoSourceTwitterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoSourceClass *source_class = MOJITO_SOURCE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoSourceTwitterPrivate));

  object_class->dispose = mojito_source_twitter_dispose;
  object_class->finalize = mojito_source_twitter_finalize;

  source_class->get_name = mojito_source_twitter_get_name;
  source_class->update = update;
}

static void
mojito_source_twitter_init (MojitoSourceTwitter *self)
{
  self->priv = GET_PRIVATE (self);

  /* TODO: get from configuration file */
  self->priv->user_id = g_strdup ("35468147630@N01");
}
