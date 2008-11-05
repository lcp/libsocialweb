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
mojito_source_initialize (MojitoSourceClass *source_class)
{
  g_return_val_if_fail (source_class->initialize, NULL);

  return source_class->initialize (source_class);
}
