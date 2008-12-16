#ifndef _MOJITO_CLIENT
#define _MOJITO_CLIENT

#include <glib-object.h>

#include <mojito-client/mojito-client-view.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_CLIENT mojito_client_get_type()

#define MOJITO_CLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_CLIENT, MojitoClient))

#define MOJITO_CLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_CLIENT, MojitoClientClass))

#define MOJITO_IS_CLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_CLIENT))

#define MOJITO_IS_CLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_CLIENT))

#define MOJITO_CLIENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_CLIENT, MojitoClientClass))

typedef struct {
  GObject parent;
} MojitoClient;

typedef struct {
  GObjectClass parent_class;
} MojitoClientClass;

GType mojito_client_get_type (void);

MojitoClient *mojito_client_new (void);

typedef void (*MojitoClientOpenViewCallback) (MojitoClient     *client,
                                              MojitoClientView *view,
                                              gpointer          userdata);

void mojito_client_open_view (MojitoClient                *client,
                              GList                       *sources,
                              guint                        count,
                              MojitoClientOpenViewCallback cb,
                              gpointer                     userdata);

typedef void (*MojitoClientGetSourcesCallback) (MojitoClient *client,
                                                GList        *sources,
                                                gpointer      userdata);

void mojito_client_get_sources (MojitoClient                  *client,
                                MojitoClientGetSourcesCallback cb,
                                gpointer                       userdata);
G_END_DECLS

#endif /* _MOJITO_CLIENT */

