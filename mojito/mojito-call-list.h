/*
 * Mojito - social data store
 * Copyright (C) 2009 Intel Corporation.
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

#include <glib.h>
#include <rest/rest-proxy.h>

typedef struct _MojitoCallList MojitoCallList;

MojitoCallList * mojito_call_list_new (void);

void mojito_call_list_free (MojitoCallList *list);

void mojito_call_list_add (MojitoCallList *list, RestProxyCall *call);

void mojito_call_list_remove (MojitoCallList *list, RestProxyCall *call);

gboolean mojito_call_list_is_empty (MojitoCallList *list);

void mojito_call_list_cancel_all (MojitoCallList *list);
