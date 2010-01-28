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

#include <config.h>
#include <glib.h>
#include <rest/rest-proxy.h>
#include "sw-call-list.h"

struct _SwCallList {
  GList *l;
};

SwCallList *
sw_call_list_new (void)
{
  SwCallList *list;

  list = g_slice_new0 (SwCallList);

  return list;
}

void
sw_call_list_free (SwCallList *list)
{
  sw_call_list_cancel_all (list);
  g_slice_free (SwCallList, list);
}

static void
call_weak_notify (gpointer data, GObject *dead_object)
{
  SwCallList *list = data;

  list->l = g_list_remove (list->l, dead_object);
}

void
sw_call_list_add (SwCallList *list, RestProxyCall *call)
{
  g_object_weak_ref (G_OBJECT (call), call_weak_notify, list);

  list->l = g_list_prepend (list->l, call);
}

void
sw_call_list_remove (SwCallList *list, RestProxyCall *call)
{
  g_object_weak_unref (G_OBJECT (call), call_weak_notify, list);

  list->l = g_list_remove (list->l, call);
}

gboolean
sw_call_list_is_empty (SwCallList *list)
{
  return list->l == NULL;
}

void
sw_call_list_cancel_all (SwCallList *list)
{
  while (list->l) {
    RestProxyCall *call = list->l->data;

    rest_proxy_call_cancel (call);
    g_object_weak_unref (G_OBJECT (call), call_weak_notify, list);
    list->l = g_list_delete_link (list->l, list->l);
  }
}
