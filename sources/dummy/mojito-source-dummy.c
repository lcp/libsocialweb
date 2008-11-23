#include "mojito-source-dummy.h"

G_DEFINE_TYPE (MojitoSourceDummy, mojito_source_dummy, MOJITO_TYPE_SOURCE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SOURCE_DUMMY, MojitoSourceDummyPrivate))

typedef struct _MojitoSourceDummyPrivate MojitoSourceDummyPrivate;

struct _MojitoSourceDummyPrivate {
};

static void
mojito_source_dummy_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  switch (property_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_source_dummy_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  switch (property_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_source_dummy_dispose (GObject *object)
{
  G_OBJECT_CLASS (mojito_source_dummy_parent_class)->dispose (object);
}

static void
mojito_source_dummy_finalize (GObject *object)
{
  G_OBJECT_CLASS (mojito_source_dummy_parent_class)->finalize (object);
}

static void
mojito_source_dummy_class_init (MojitoSourceDummyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoSourceDummyPrivate));

  object_class->get_property = mojito_source_dummy_get_property;
  object_class->set_property = mojito_source_dummy_set_property;
  object_class->dispose = mojito_source_dummy_dispose;
  object_class->finalize = mojito_source_dummy_finalize;
}

static void
mojito_source_dummy_init (MojitoSourceDummy *self)
{
}

MojitoSourceDummy*
mojito_source_dummy_new (void)
{
  return g_object_new (MOJITO_TYPE_SOURCE_DUMMY, NULL);
}


