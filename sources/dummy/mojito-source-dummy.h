#ifndef _MOJITO_SOURCE_DUMMY
#define _MOJITO_SOURCE_DUMMY

#include <glib-object.h>

#include "mojito-source.h"

G_BEGIN_DECLS

#define MOJITO_TYPE_SOURCE_DUMMY mojito_source_dummy_get_type()

#define MOJITO_SOURCE_DUMMY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_SOURCE_DUMMY, MojitoSourceDummy))

#define MOJITO_SOURCE_DUMMY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_SOURCE_DUMMY, MojitoSourceDummyClass))

#define MOJITO_IS_SOURCE_DUMMY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_SOURCE_DUMMY))

#define MOJITO_IS_SOURCE_DUMMY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_SOURCE_DUMMY))

#define MOJITO_SOURCE_DUMMY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_SOURCE_DUMMY, MojitoSourceDummyClass))

typedef struct {
  MojitoSource parent;
} MojitoSourceDummy;

typedef struct {
  MojitoSourceClass parent_class;
} MojitoSourceDummyClass;

GType mojito_source_dummy_get_type (void);

MojitoSourceDummy *mojito_source_dummy_new (void);

G_END_DECLS

#endif /* _MOJITO_SOURCE_DUMMY */

