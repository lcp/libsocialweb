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

#ifndef _TEST_RUNNER
#define _TEST_RUNNER

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _DummyObject DummyObject;
typedef struct _DummyObjectClass DummyObjectClass;
#define TYPE_DUMMY_OBJECT dummy_object_get_type()
GType dummy_object_get_type (void);
DummyObject * dummy_object_new (void);

G_END_DECLS

#endif /* _TEST_RUNNER */
