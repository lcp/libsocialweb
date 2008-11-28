#include "mojito-source.h"
#include "mojito-marshals.h"
#include "mojito-core.h"

G_DEFINE_ABSTRACT_TYPE (MojitoSource, mojito_source, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SOURCE, MojitoSourcePrivate))

typedef struct _MojitoSourcePrivate MojitoSourcePrivate;

struct _MojitoSourcePrivate {
  MojitoCore *core;
};

enum {
  ITEM_ADDED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_CORE
};

static guint signals[LAST_SIGNAL];

static void
mojito_source_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  MojitoSourcePrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_CORE:
      priv->core = g_value_dup_object (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_source_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  MojitoSourcePrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_CORE:
      g_value_set_object (value, priv->core);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_source_dispose (GObject *object)
{
  MojitoSourcePrivate *priv = GET_PRIVATE (object);

  if (priv->core)
  {
    g_object_unref (priv->core);
    priv->core = NULL;
  }

  G_OBJECT_CLASS (mojito_source_parent_class)->dispose (object);
}

static void
mojito_source_class_init (MojitoSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MojitoSourcePrivate));

  object_class->get_property = mojito_source_get_property;
  object_class->set_property = mojito_source_set_property;
  object_class->dispose = mojito_source_dispose;

  pspec = g_param_spec_object ("core",
                               "core",
                               "The daemon's warp core",
                               MOJITO_TYPE_CORE,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_CORE, pspec);

  /* use DBUS_TYPE_G_STRING_STRING_HASHTABLE? */
  signals[ITEM_ADDED] =
    g_signal_new ("item-added",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  mojito_marshal_VOID__STRING_INT64_BOXED,
                  G_TYPE_NONE, 3, 
                  G_TYPE_STRING,
                  G_TYPE_INT64,
                  G_TYPE_HASH_TABLE);
}

static void
mojito_source_init (MojitoSource *self)
{
}

void
mojito_source_start (MojitoSource *source)
{
  MojitoSourceClass *source_class = MOJITO_SOURCE_GET_CLASS (source);

  g_return_if_fail (source_class->start);

  source_class->start (source);
}

char *
mojito_source_get_name (MojitoSource *source)
{
  MojitoSourceClass *source_class = MOJITO_SOURCE_GET_CLASS (source);

  g_return_val_if_fail (source_class->get_name, NULL);

  return source_class->get_name (source);
}

MojitoCore *
mojito_source_get_core (MojitoSource *source)
{
  return GET_PRIVATE(source)->core;
}
