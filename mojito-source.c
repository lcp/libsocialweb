#include "mojito-source.h"
#include "marshals.h"

/* TODO: make this an interface? */

G_DEFINE_ABSTRACT_TYPE (MojitoSource, mojito_source, G_TYPE_OBJECT)

enum {
  ITEM_ADDED,
  LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

static void
mojito_source_class_init (MojitoSourceClass *klass)
{
  signals[ITEM_ADDED] =
    g_signal_new ("item-added",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST|G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE,
                  1, G_TYPE_HASH_TABLE);
  /* use DBUS_TYPE_G_STRING_STRING_HASHTABLE? */
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
mojito_source_start (MojitoSource *source)
{
  MojitoSourceClass *source_class = MOJITO_SOURCE_GET_CLASS (source);

  g_return_if_fail (source_class->start);

  source_class->start (source);
}
