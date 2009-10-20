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

#include <config.h>
#include <glib.h>
#include <rest/rest-proxy.h>
#include "mojito-call-list.h"

struct _MojitoCallList {
  GList *l;
};

MojitoCallList *
mojito_call_list_new (void)
{
  MojitoCallList *list;

  list = g_slice_new0 (MojitoCallList);

  return list;
}

void
mojito_call_list_free (MojitoCallList *list)
{
  mojito_call_list_cancel_all (list);
  g_slice_free (MojitoCallList, list);
}

static void
call_weak_notify (gpointer data, GObject *dead_object)
{
  MojitoCallList *list = data;

  list->l = g_list_remove (list->l, dead_object);
}

void
mojito_call_list_add (MojitoCallList *list, RestProxyCall *call)
{
  g_object_weak_ref (G_OBJECT (call), call_weak_notify, list);

  list->l = g_list_prepend (list->l, call);
}

void
mojito_call_list_remove (MojitoCallList *list, RestProxyCall *call)
{
  g_object_weak_unref (G_OBJECT (call), call_weak_notify, list);

  list->l = g_list_remove (list->l, call);
}

gboolean
mojito_call_list_is_empty (MojitoCallList *list)
{
  return list->l == NULL;
}

void
mojito_call_list_cancel_all (MojitoCallList *list)
{
  while (list->l) {
    RestProxyCall *call = list->l->data;

    rest_proxy_call_cancel (call);
    g_object_weak_unref (G_OBJECT (call), call_weak_notify, list);
    list->l = g_list_delete_link (list->l, list->l);
  }
}
