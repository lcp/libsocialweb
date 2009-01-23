/*
 * Mojito - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef _MOJITO_SOURCE
#define _MOJITO_SOURCE

#include <glib-object.h>
#include "mojito-core.h"
#include "mojito-set.h"

G_BEGIN_DECLS

#define MOJITO_TYPE_SOURCE mojito_source_get_type()

#define MOJITO_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOJITO_TYPE_SOURCE, MojitoSource))

#define MOJITO_SOURCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MOJITO_TYPE_SOURCE, MojitoSourceClass))

#define MOJITO_IS_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOJITO_TYPE_SOURCE))

#define MOJITO_IS_SOURCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOJITO_TYPE_SOURCE))

#define MOJITO_SOURCE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOJITO_TYPE_SOURCE, MojitoSourceClass))

typedef struct {
  GObject parent;
} MojitoSource;

typedef void (*MojitoSourceDataFunc) (MojitoSource *source, MojitoSet *set, gpointer user_data);

typedef struct _MojitoSourceClass MojitoSourceClass;
struct _MojitoSourceClass {
  GObjectClass parent_class;
  const char * (*get_name) (MojitoSource *source);
  void (*update) (MojitoSource *source, MojitoSourceDataFunc callback, gpointer user_data);
};

GType mojito_source_get_type (void);

MojitoCore *mojito_source_get_core (MojitoSource *source);

const char * mojito_source_get_name (MojitoSource *source);

/* Please update yourself */
void mojito_source_update (MojitoSource *source, MojitoSourceDataFunc callback, gpointer user_data);

MojitoSet * mojito_source_get_items (MojitoSource *source);

G_END_DECLS

#endif /* _MOJITO_SOURCE */

