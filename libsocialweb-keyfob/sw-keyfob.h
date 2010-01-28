#ifndef __SW_KEYFOB_H__
#define __SW_KEYFOB_H__

#include <rest/oauth-proxy.h>
#include <rest-extras/flickr-proxy.h>

G_BEGIN_DECLS

/* Generic callback */
typedef void (*SwKeyfobCallback) (RestProxy *proxy, gboolean authorised, gpointer user_data);

void sw_keyfob_oauth (OAuthProxy *proxy,
                          SwKeyfobCallback callback,
                          gpointer user_data);

gboolean sw_keyfob_oauth_sync (OAuthProxy *proxy);

void sw_keyfob_flickr (FlickrProxy *proxy,
                          SwKeyfobCallback callback,
                          gpointer user_data);

gboolean sw_keyfob_flickr_sync (FlickrProxy *proxy);

G_END_DECLS

#endif /* __SW_KEYFOB_H__ */
