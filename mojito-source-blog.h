#ifndef _MOJITO_SOURCE_BLOG
#define _MOJITO_SOURCE_BLOG

#include "mojito-core.h"
#include "mojito-source.h"

G_BEGIN_DECLS

#define MOJITO_TYPE_SOURCE_BLOG mojito_source_blog_get_type()

#define MOJITO_SOURCE_BLOG(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_SOURCE_BLOG, MojitoSourceBlog))

#define MOJITO_SOURCE_BLOG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_SOURCE_BLOG, MojitoSourceBlogClass))

#define MOJITO_IS_SOURCE_BLOG(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_SOURCE_BLOG))

#define MOJITO_IS_SOURCE_BLOG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_SOURCE_BLOG))

#define MOJITO_SOURCE_BLOG_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_SOURCE_BLOG, MojitoSourceBlogClass))

typedef struct _MojitoSourceBlogPrivate MojitoSourceBlogPrivate;

typedef struct {
  MojitoSource parent;
  MojitoSourceBlogPrivate *priv;
} MojitoSourceBlog;

typedef struct {
  MojitoSourceClass parent_class;
} MojitoSourceBlogClass;

GType mojito_source_blog_get_type (void);

MojitoSource * mojito_source_blog_new (MojitoCore *core);

G_END_DECLS

#endif /* _MOJITO_SOURCE_BLOG */
