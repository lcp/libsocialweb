#ifndef _MOJITO_VIEW
#define _MOJITO_VIEW

#include <glib-object.h>
#include "mojito-source.h"

G_BEGIN_DECLS

#define MOJITO_TYPE_VIEW mojito_view_get_type()

#define MOJITO_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_VIEW, MojitoView))

#define MOJITO_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_VIEW, MojitoViewClass))

#define MOJITO_IS_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_VIEW))

#define MOJITO_IS_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_VIEW))

#define MOJITO_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_VIEW, MojitoViewClass))

typedef struct _MojitoViewPrivate MojitoViewPrivate;

typedef struct {
  GObject parent;
  MojitoViewPrivate *priv;
} MojitoView;

typedef struct {
  GObjectClass parent_class;
} MojitoViewClass;

GType mojito_view_get_type (void);

MojitoView* mojito_view_new (guint count);

void mojito_view_add_source (MojitoView *view, MojitoSource *source);

G_END_DECLS

#endif /* _MOJITO_VIEW */
