#include "mojito-core.h"

void mojito_photos_initialize (MojitoCore *core);
void mojito_photos_add (MojitoCore *core, const char *source_id, const char *item_id, time_t date, const char *link, const char *title);
void mojito_photos_remove (MojitoCore *core, const char *source_id);
