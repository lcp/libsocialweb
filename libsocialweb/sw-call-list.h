/*
 * libsocialweb - social data store
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

typedef struct _SwCallList SwCallList;

SwCallList * sw_call_list_new (void);

void sw_call_list_free (SwCallList *list);

void sw_call_list_add (SwCallList *list, RestProxyCall *call);

void sw_call_list_remove (SwCallList *list, RestProxyCall *call);

gboolean sw_call_list_is_empty (SwCallList *list);

void sw_call_list_cancel_all (SwCallList *list);
