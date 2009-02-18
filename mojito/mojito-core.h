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

#ifndef _MOJITO_CORE
#define _MOJITO_CORE

#include <glib-object.h>
#include <mojito/mojito-types.h>

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

struct _MojitoCore {
  GObject parent;
  MojitoCorePrivate *priv;
};

typedef struct {
  GObjectClass parent_class;
} MojitoCoreClass;

GType mojito_core_get_type (void);

MojitoCore* mojito_core_new (void);

void mojito_core_run (MojitoCore *core);

G_END_DECLS

#endif /* _MOJITO_CORE */
