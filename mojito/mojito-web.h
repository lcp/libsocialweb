#include "mojito-core.h"
#include <libsoup/soup.h>

guint mojito_web_cached_send (MojitoCore *core, SoupSession *session, SoupMessage *msg);

char * mojito_web_download_image (SoupSession *session, const char *url);
