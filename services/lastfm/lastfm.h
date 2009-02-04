#ifndef _MOJITO_SERVICE_LASTFM
#define _MOJITO_SERVICE_LASTFM

#include <glib-object.h>
#include <mojito/mojito-service.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_SERVICE_LASTFM mojito_service_lastfm_get_type()

#define MOJITO_SERVICE_LASTFM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_SERVICE_LASTFM, MojitoServiceLastfm))

#define MOJITO_SERVICE_LASTFM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_SERVICE_LASTFM, MojitoServiceLastfmClass))

#define MOJITO_IS_SERVICE_LASTFM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_SERVICE_LASTFM))

#define MOJITO_IS_SERVICE_LASTFM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_SERVICE_LASTFM))

#define MOJITO_SERVICE_LASTFM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_SERVICE_LASTFM, MojitoServiceLastfmClass))

typedef struct _MojitoServiceLastfmPrivate MojitoServiceLastfmPrivate;

typedef struct {
  MojitoService parent;
  MojitoServiceLastfmPrivate *priv;
} MojitoServiceLastfm;

typedef struct {
  MojitoServiceClass parent_class;
} MojitoServiceLastfmClass;

GType mojito_service_lastfm_get_type (void);

G_END_DECLS

#endif /* _MOJITO_SERVICE_LASTFM */
