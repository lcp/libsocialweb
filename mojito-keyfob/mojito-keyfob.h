#ifndef __MOJITO_KEYFOB_H__
#define __MOJITO_KEYFOB_H__

#include <rest/oauth-proxy.h>

G_BEGIN_DECLS

/* Generic callback */
typedef void (*MojitoKeyfobCallback) (RestProxy *proxy, gboolean authorised, gpointer user_data);

void mojito_keyfob_oauth (OAuthProxy *proxy,
                          MojitoKeyfobCallback callback,
                          gpointer user_data);

gboolean mojito_keyfob_oauth_sync (OAuthProxy *proxy);

G_END_DECLS

#endif /* __MOJITO_KEYFOB_H__ */
