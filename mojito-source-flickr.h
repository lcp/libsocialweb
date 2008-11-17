#ifndef _MOJITO_SOURCE_FLICKR
#define _MOJITO_SOURCE_FLICKR

#include "mojito-core.h"
#include "mojito-source.h"

G_BEGIN_DECLS

#define MOJITO_TYPE_SOURCE_FLICKR mojito_source_flickr_get_type()

#define MOJITO_SOURCE_FLICKR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_SOURCE_FLICKR, MojitoSourceFlickr))

#define MOJITO_SOURCE_FLICKR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_SOURCE_FLICKR, MojitoSourceFlickrClass))

#define MOJITO_IS_SOURCE_FLICKR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_SOURCE_FLICKR))

#define MOJITO_IS_SOURCE_FLICKR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_SOURCE_FLICKR))

#define MOJITO_SOURCE_FLICKR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_SOURCE_FLICKR, MojitoSourceFlickrClass))

typedef struct _MojitoSourceFlickrPrivate MojitoSourceFlickrPrivate;

typedef struct {
  MojitoSource parent;
  MojitoSourceFlickrPrivate *priv;
} MojitoSourceFlickr;

typedef struct {
  MojitoSourceClass parent_class;
} MojitoSourceFlickrClass;

GType mojito_source_flickr_get_type (void);

MojitoSource * mojito_source_flickr_new (MojitoCore *core);

G_END_DECLS

#endif /* _MOJITO_SOURCE_FLICKR */
