#ifndef _MOJITO_SOURCE_SKELETON
#define _MOJITO_SOURCE_SKELETON

#include "mojito-source.h"

G_BEGIN_DECLS

#define MOJITO_TYPE_SOURCE_SKELETON mojito_source_skeleton_get_type()

#define MOJITO_SOURCE_SKELETON(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_SOURCE_SKELETON, MojitoSourceSkeleton))

#define MOJITO_SOURCE_SKELETON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_SOURCE_SKELETON, MojitoSourceSkeletonClass))

#define MOJITO_IS_SOURCE_SKELETON(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_SOURCE_SKELETON))

#define MOJITO_IS_SOURCE_SKELETON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_SOURCE_SKELETON))

#define MOJITO_SOURCE_SKELETON_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_SOURCE_SKELETON, MojitoSourceSkeletonClass))

typedef struct _MojitoSourceSkeletonPrivate MojitoSourceSkeletonPrivate;

typedef struct {
  MojitoSource parent;
  MojitoSourceSkeletonPrivate *priv;
} MojitoSourceSkeleton;

typedef struct {
  MojitoSourceClass parent_class;
} MojitoSourceSkeletonClass;

GType mojito_source_skeleton_get_type (void);

G_END_DECLS

#endif /* _MOJITO_SOURCE_SKELETON */
