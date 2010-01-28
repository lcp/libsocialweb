/*
 * libsocialweb - social data store
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

#ifndef _SW_CORE
#define _SW_CORE

#include <glib-object.h>
#include <libsocialweb/sw-types.h>
#include <dbus/dbus-glib-lowlevel.h>

G_BEGIN_DECLS

#define SW_TYPE_CORE sw_core_get_type()

#define SW_CORE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_CORE, SwCore))

#define SW_CORE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_CORE, SwCoreClass))

#define SW_IS_CORE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_CORE))

#define SW_IS_CORE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_CORE))

#define SW_CORE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_CORE, SwCoreClass))

typedef struct _SwCorePrivate SwCorePrivate;

struct _SwCore {
  GObject parent;
  SwCorePrivate *priv;
};

typedef struct {
  GObjectClass parent_class;
} SwCoreClass;

GType sw_core_get_type (void);

SwCore* sw_core_new (void);

void sw_core_run (SwCore *core);

gboolean sw_core_is_item_banned (SwCore *core,
                                 SwItem *item);

SwCore *sw_core_dup_singleton (void);

DBusGConnection *sw_core_get_connection (SwCore *core);

G_END_DECLS

#endif /* _SW_CORE */
