#ifndef _MOJITO_CORE
#define _MOJITO_CORE

#include <glib-object.h>
#include <libsoup/soup.h>
#include <sqlite3.h>

G_BEGIN_DECLS

#define MOJITO_TYPE_CORE mojito_core_get_type()

#define MOJITO_CORE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_CORE, MojitoCore))

#define MOJITO_CORE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_CORE, MojitoCoreClass))

#define MOJITO_IS_CORE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_CORE))

#define MOJITO_IS_CORE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_CORE))

#define MOJITO_CORE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_CORE, MojitoCoreClass))

typedef struct _MojitoCorePrivate MojitoCorePrivate;

typedef struct {
  GObject parent;
  MojitoCorePrivate *priv;
} MojitoCore;

typedef struct {
  GObjectClass parent_class;
} MojitoCoreClass;

GType mojito_core_get_type (void);

MojitoCore* mojito_core_new (void);

void mojito_core_run (MojitoCore *core);

sqlite3 *mojito_core_get_db (MojitoCore *core);

SoupSession *mojito_core_get_session (MojitoCore *core);

void mojito_core_add_item (MojitoCore *core, const char *source_id, const char *item_id, time_t date, const char *link, const char *title);

void mojito_core_remove_items (MojitoCore *core, const char *source_id);

G_END_DECLS

#endif /* _MOJITO_CORE */
