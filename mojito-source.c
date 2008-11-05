#include "mojito-source.h"

G_DEFINE_ABSTRACT_TYPE (MojitoSource, mojito_source, G_TYPE_OBJECT)

static void
mojito_source_class_init (MojitoSourceClass *klass)
{
}

static void
mojito_source_init (MojitoSource *self)
{
}

GList *
mojito_source_initialize (MojitoSourceClass *source_class, MojitoCore *core)
{
  g_return_val_if_fail (source_class->initialize, NULL);

  return source_class->initialize (source_class, core);
}

void
mojito_source_update (MojitoSource *source)
{
  MojitoSourceClass *source_class = MOJITO_SOURCE_GET_CLASS (source);

  g_return_if_fail (source_class->update);

  source_class->update (source);
}
