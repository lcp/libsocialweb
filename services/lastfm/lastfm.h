#ifndef _MOJITO_SOURCE_LASTFM
#define _MOJITO_SOURCE_LASTFM

#include <glib-object.h>
#include <mojito/mojito-source.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_SOURCE_LASTFM mojito_source_lastfm_get_type()

#define MOJITO_SOURCE_LASTFM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_SOURCE_LASTFM, MojitoSourceLastfm))

#define MOJITO_SOURCE_LASTFM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_SOURCE_LASTFM, MojitoSourceLastfmClass))

#define MOJITO_IS_SOURCE_LASTFM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_SOURCE_LASTFM))

#define MOJITO_IS_SOURCE_LASTFM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_SOURCE_LASTFM))

#define MOJITO_SOURCE_LASTFM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_SOURCE_LASTFM, MojitoSourceLastfmClass))

typedef struct _MojitoSourceLastfmPrivate MojitoSourceLastfmPrivate;

typedef struct {
  MojitoSource parent;
  MojitoSourceLastfmPrivate *priv;
} MojitoSourceLastfm;

typedef struct {
  MojitoSourceClass parent_class;
} MojitoSourceLastfmClass;

GType mojito_source_lastfm_get_type (void);

G_END_DECLS

#endif /* _MOJITO_SOURCE_LASTFM */
