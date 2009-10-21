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

#include <math.h>
#include "mojito-view.h"
#include "mojito-view-ginterface.h"

#include <mojito/mojito-core.h>
#include <mojito/mojito-debug.h>
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>
#include <mojito/mojito-utils.h>
#include <mojito/mojito-cache.h>
#include <mojito/mojito-online.h>

static void view_iface_init (gpointer g_iface, gpointer iface_data);
G_DEFINE_TYPE_WITH_CODE (MojitoView, mojito_view, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MOJITO_TYPE_VIEW_IFACE, view_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_VIEW, MojitoViewPrivate))

/* Refresh every 5 minutes */
#define REFRESH_TIMEOUT (5 * 60)

struct _MojitoViewPrivate {
  MojitoCore *core;
  /* List of MojitoService objects */
  GList *services;
  /* The maximum number of items in the view */
  guint count;
  /* The refresh timeout id */
  guint refresh_timeout_id;
  /* The set of all items currently being considered for the client */
  MojitoSet *all_items;
  /* The items we've sent to the client */
  MojitoSet *current;
  /* Whether we're currently running */
  gboolean running;
};

enum {
  PROP_0,
  PROP_CORE,
  PROP_COUNT,
};

static int
get_service_count (GHashTable *hash, MojitoService *service)
{
  return GPOINTER_TO_INT (g_hash_table_lookup (hash, service));
}

static void
inc_service_count (GHashTable *hash, MojitoService *service)
{
  int count;
  count = GPOINTER_TO_INT (g_hash_table_lookup (hash, service));
  ++count;
  g_hash_table_insert (hash, service, GINT_TO_POINTER (count));
}

static void
send_added (gpointer data, gpointer user_data)
{
  MojitoItem *item = data;
  MojitoView *view = user_data;
  MojitoService *service;
  time_t time;

  service = mojito_item_get_service (item);

  time = mojito_time_t_from_string (mojito_item_get (item, "date"));

  mojito_view_iface_emit_item_added (view,
                                     mojito_service_get_name (service),
                                     mojito_item_get (item, "id"),
                                     time,
                                     mojito_item_peek_hash (item));
}

static void
send_removed (gpointer data, gpointer user_data)
{
  MojitoItem *item = data;
  MojitoView *view = user_data;
  MojitoService *service;

  service = mojito_item_get_service (item);

  mojito_view_iface_emit_item_removed (view,
                                       mojito_service_get_name (service),
                                       mojito_item_get (item, "id"));
}

static MojitoSet *
munge_items (MojitoView *view)
{
  MojitoViewPrivate *priv = view->priv;
  GList *list, *l = NULL;
  int count, service_max;
  GHashTable *counts;
  MojitoSet *new;
  MojitoItem *item;

  /* The magic */
  list = mojito_set_as_list (priv->all_items);

  while (list) {
    item = (MojitoItem *)list->data;
    if (!mojito_core_is_item_banned (priv->core, item)) {

      if (mojito_item_get_ready (item)) {
        l = g_list_prepend (l, item);
        MOJITO_DEBUG (VIEWS, "Item ready: %s",
                      mojito_item_get (item, "id"));
      } else {
        MOJITO_DEBUG (VIEWS, "Item not ready. Skipping: %s",
                      mojito_item_get (item, "id"));
      }
    }
    list = g_list_delete_link (list, list);
  }
  list = l;

  list = g_list_sort (list, (GCompareFunc)mojito_item_compare_date_newer);

  counts = g_hash_table_new (NULL, NULL);
  service_max = ceil ((float)priv->count / g_list_length (priv->services));

  count = 0;
  new = mojito_item_set_new ();

  /* We manipulate list in place instead of using a temporary GList* because
   * need to keep track of the new head.  This loop exits if we have ran out of
   * items from the sources (list == NULL), we have filled up the result list
   * (count >= priv->count), or if we have reached the end of the list but have
   * not yet filled up the result list.
  */
  /* TODO: use gsequence instead for clarity? */
  while (list && count < priv->count) {
    MojitoItem *item;
    MojitoService *service;

    item = list->data;
    service = mojito_item_get_service (item);

    if (get_service_count (counts, service) >= service_max) {
      if (list->next) {
        list = list->next;
        continue;
      } else {
        /* Here we have got to the end of the source list but haven't yet filled
           up the target list */
        /* TODO: put the fill-in logic here? */
        break;
      }
    }

    /* New list steals the reference */
    count++;
    mojito_set_add (new, (GObject*)item);
    g_object_unref (item);
    list = g_list_delete_link (list, list);

    inc_service_count (counts, service);
  }

  /* If we still have items left in the pending list, rewind back to the
     beginning and add more */
  list = g_list_first (list);
  while (list && count < priv->count) {
    GObject *item = list->data;
    count++;
    mojito_set_add (new, item);
    g_object_unref (item);
    list = g_list_delete_link (list, list);
  }

  /* Clean up */
  while (list) {
    g_object_unref (list->data);
    list = g_list_delete_link (list, list);
  }

  return new;
}

static gboolean
remove_service (GObject *object, gpointer user_data)
{
  MojitoItem *item = MOJITO_ITEM (object);
  MojitoService *service = MOJITO_SERVICE (user_data);

  return mojito_item_get_service (item) == service;
}

void
mojito_view_recalculate (MojitoView *view)
{
  MojitoViewPrivate *priv;
  MojitoSet *old_items, *new_items;
  MojitoSet *removed_items, *added_items;

  g_return_if_fail (MOJITO_IS_VIEW (view));

  priv = view->priv;
  if (!priv->running)
    return;

  old_items = priv->current;
  new_items = munge_items (view);

  removed_items = mojito_set_difference (old_items, new_items);
  added_items = mojito_set_difference (new_items, old_items);

  mojito_set_foreach (removed_items, (GFunc)send_removed, view);
  mojito_set_foreach (added_items, (GFunc)send_added, view);

  mojito_set_unref (removed_items);
  mojito_set_unref (added_items);

  mojito_set_unref (priv->current);
  priv->current = new_items;
}

static void
_item_ready_notify_cb (MojitoItem *item,
                       GParamSpec *pspec,
                       MojitoView *view)
{
  /* TODO: Use a timeout to rate limit this */
  if (mojito_item_get_ready (item)) {
    MOJITO_DEBUG (VIEWS, "Item became ready: %s.",
                  mojito_item_get (item, "id"));
    mojito_view_recalculate (view);
    g_signal_handlers_disconnect_by_func (item,
                                          _item_ready_notify_cb,
                                          view);
  }
}

static void
setup_ready_handler (gpointer object, gpointer user_data)
{
  MojitoItem *item = MOJITO_ITEM (object);
  MojitoView *view = MOJITO_VIEW (user_data);

  if (mojito_item_get_ready (item))
    return;

  MOJITO_DEBUG (VIEWS, "Item not ready. Setting up signal handler: %s.",
                mojito_item_get (item, "id"));

  g_signal_connect (item,
                    "notify::ready",
                    (GCallback)_item_ready_notify_cb,
                    view);
}

static void
service_updated (MojitoService *service, MojitoSet *set, gpointer user_data)
{
  MojitoView *view = MOJITO_VIEW (user_data);
  MojitoViewPrivate *priv = view->priv;

  if (set) {
    mojito_cache_save (service, set);
  } else {
    mojito_cache_drop (service);
  }

  /* Remove all existing items from this service */
  mojito_set_foreach_remove (priv->all_items, remove_service, service);
  if (set) {
    mojito_set_add_from (priv->all_items, set);
    mojito_set_foreach (set, setup_ready_handler, view);
    mojito_set_unref (set);
  }

  mojito_view_recalculate (view);
}

/*
 * Called in a timeout and asks all of the sources to refresh themselves.
 */
static gboolean
start_refresh (MojitoView *view)
{
  MojitoViewPrivate *priv = view->priv;
  GList *l;

  for (l = priv->services; l; l = l->next) {
    MojitoService *service = l->data;
    MOJITO_DEBUG (VIEWS, "Refreshing %s on view %p",
                  mojito_service_get_name (service),
                  view);
    mojito_service_refresh (service);
  }

  return TRUE;
}

static void
load_cache (MojitoView *view)
{
  MojitoViewPrivate *priv = view->priv;
  GList *l;

  for (l = priv->services; l; l = l->next) {
    MojitoService *service = l->data;
    service_updated (service, mojito_cache_load (service), view);
  }
}

static void
install_refresh_timeout (MojitoView *view)
{
  if (view->priv->refresh_timeout_id == 0)
    view->priv->refresh_timeout_id = g_timeout_add_seconds (REFRESH_TIMEOUT, (GSourceFunc)start_refresh, view);
}

static void
remove_refresh_timeout (MojitoView *view)
{
  if (view->priv->refresh_timeout_id) {
    g_source_remove (view->priv->refresh_timeout_id);
    view->priv->refresh_timeout_id = 0;
  }
}

static void
online_notify (gboolean online, gpointer user_data)
{
  MojitoView *view = user_data;

  if (online) {
    install_refresh_timeout (view);
    if (view->priv->running) {
      load_cache (view);
      start_refresh (view);
    }
  } else {
    remove_refresh_timeout (view);
  }
}

static void
view_start (MojitoViewIface *iface, DBusGMethodInvocation *context)
{
  MojitoView *view = MOJITO_VIEW (iface);
  GList *l;

  view->priv->running = TRUE;

  mojito_view_iface_return_from_start (context);

  /* Tell the services to start */
  for (l = view->priv->services; l; l = l->next) {
    MojitoService *service = l->data;
    mojito_service_start (service);
  }

  if (mojito_is_online ()) {
    online_notify (TRUE, view);
  } else {
    online_notify (FALSE, view);
    /* Online notify doesn't load the cache if we are offline, so do that now */
    load_cache (view);
  }
}

static void
view_refresh (MojitoViewIface *iface, DBusGMethodInvocation *context)
{
  MojitoView *view = MOJITO_VIEW (iface);

  /* Reinstall the timeout */
  remove_refresh_timeout (view);
  install_refresh_timeout (view);

  mojito_view_iface_return_from_refresh (context);

  start_refresh (view);
}

static void
stop (MojitoView *view)
{
  view->priv->running = FALSE;

  remove_refresh_timeout (view);
}

static void
view_stop (MojitoViewIface *iface, DBusGMethodInvocation *context)
{
  MojitoView *view = MOJITO_VIEW (iface);

  stop (view);

  mojito_view_iface_return_from_stop (context);
}

static void
view_close (MojitoViewIface *iface, DBusGMethodInvocation *context)
{
  MojitoView *view = MOJITO_VIEW (iface);

  /* Explicitly stop the view in case there are pending updates in progress */
  stop (view);

  g_object_unref (view);

  mojito_view_iface_return_from_close (context);
}

static void
mojito_view_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  MojitoViewPrivate *priv = MOJITO_VIEW (object)->priv;

  switch (property_id) {
  case PROP_CORE:
    g_value_set_object (value, priv->core);
    break;
  case PROP_COUNT:
    g_value_set_uint (value, priv->count);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_view_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  MojitoViewPrivate *priv = MOJITO_VIEW (object)->priv;

  switch (property_id) {
  case PROP_CORE:
    priv->core = g_value_dup_object (value);
    break;
  case PROP_COUNT:
    priv->count = g_value_get_uint (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_view_dispose (GObject *object)
{
  MojitoView *view = MOJITO_VIEW (object);
  MojitoViewPrivate *priv = view->priv;

  if (priv->core) {
    g_object_unref (priv->core);
    priv->core = NULL;
  }

  if (priv->all_items) {
    mojito_set_unref (priv->all_items);
    priv->all_items = NULL;
  }

  if (priv->current) {
    mojito_set_unref (priv->current);
    priv->current = NULL;
  }

  while (priv->services) {
    MojitoService *service = priv->services->data;
    g_object_unref (service);
    priv->services = g_list_delete_link (priv->services, priv->services);
  }

  G_OBJECT_CLASS (mojito_view_parent_class)->dispose (object);
}


static void
mojito_view_finalize (GObject *object)
{
  MojitoView *view = MOJITO_VIEW (object);

  stop (view);

  mojito_online_remove_notify (online_notify, view);

  G_OBJECT_CLASS (mojito_view_parent_class)->finalize (object);
}

static void
view_iface_init (gpointer g_iface, gpointer iface_data)
{
  MojitoViewIfaceClass *klass = (MojitoViewIfaceClass*)g_iface;
  mojito_view_iface_implement_start (klass, view_start);
  mojito_view_iface_implement_refresh (klass, view_refresh);
  mojito_view_iface_implement_stop (klass, view_stop);
  mojito_view_iface_implement_close (klass, view_close);
}

static void
mojito_view_class_init (MojitoViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MojitoViewPrivate));

  object_class->get_property = mojito_view_get_property;
  object_class->set_property = mojito_view_set_property;
  object_class->dispose = mojito_view_dispose;
  object_class->finalize = mojito_view_finalize;

  pspec = g_param_spec_object ("core", "core", "The core",
                               MOJITO_TYPE_CORE,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_CORE, pspec);

  pspec = g_param_spec_uint ("count", "count", "The number of items",
                             1, G_MAXUINT, 10,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_COUNT, pspec);
}

static void
mojito_view_init (MojitoView *self)
{
  self->priv = GET_PRIVATE (self);

  self->priv->all_items = mojito_item_set_new ();
  self->priv->current = mojito_item_set_new ();

  self->priv->running = FALSE;

  mojito_online_add_notify (online_notify, self);
}

MojitoView*
mojito_view_new (MojitoCore *core, guint count)
{
  return g_object_new (MOJITO_TYPE_VIEW,
                       "core", core,
                       "count", count,
                       NULL);
}

/* This adopts the reference passed */
void
mojito_view_add_service (MojitoView *view, MojitoService *service, GHashTable *params)
{
  MojitoViewPrivate *priv = view->priv;

  priv->services = g_list_append (priv->services, service);
  /* TODO: use _object etc */
  g_signal_connect (service, "refreshed", G_CALLBACK (service_updated), view);
}


#if BUILD_TESTS

#include <services/dummy/mojito-service-dummy.h>

void
test_view_new (void)
{
  MojitoView *view;
  view = mojito_view_new (NULL, 5);
  g_object_unref (view);
}

#if 0
static MojitoItem *
make_item_dated (MojitoService *service, time_t date)
{
  MojitoItem *item;
  char *id;

  g_assert (service);

  id = g_strdup_printf ("random-item-%d", g_test_rand_int_range (0, G_MAXINT));

  item = mojito_item_new ();
  mojito_item_set_service (item, service);
  mojito_item_put (item, "id", id);
  mojito_item_take (item, "title", id);
  mojito_item_put (item, "date", mojito_time_t_to_string (date));

  return item;
}

static MojitoItem *
make_item (MojitoService *service)
{
  return make_item_dated (service, time (NULL));
}
#endif

/*
 * Create a view of 5 items on a single service, add 5 items to the pending
 * list, and check that they are all members of the result.
 */
void
test_view_munge_1 (void)
{
  g_message ("This test is disabled");
#if 0
  MojitoView *view;
  MojitoViewPrivate *priv;
  MojitoService *service;
  GList *items = NULL;
  MojitoSet *set;
  int i;

  view = mojito_view_new (5);
  priv = view->priv;

  service = g_object_new (MOJITO_TYPE_SERVICE_DUMMY, NULL);
  mojito_view_add_service (view, service, NULL);

  for (i = 0; i < 5; i++) {
    MojitoItem *item;
    item = make_item (service);
    items = g_list_prepend (items, item);
    mojito_set_add (priv->pending_items, G_OBJECT (item));
  }

  set = munge_items (view);

  g_assert (mojito_set_size (set) == 5);

  while (items) {
    g_assert (mojito_set_has (set, items->data));
    items = items->next;
  }

  g_object_unref (view);
#endif
}

/*
 * Create a view of 1 item on a single service, add 2 items to the pending
 * list, and check that the latest item was selected.
 */
void
test_view_munge_2 (void)
{
  g_message ("This test is disabled");
#if 0
  MojitoView *view;
  MojitoViewPrivate *priv;
  MojitoService *service;
  MojitoItem *old_item, *new_item;
  MojitoSet *set;

  view = mojito_view_new (1);
  priv = view->priv;

  service = g_object_new (MOJITO_TYPE_SERVICE_DUMMY, NULL);
  mojito_view_add_service (view, service, NULL);

  old_item = make_item_dated (service, 1);
  mojito_set_add (priv->pending_items, G_OBJECT (old_item));

  new_item = make_item_dated (service, 1000);
  mojito_set_add (priv->pending_items, G_OBJECT (new_item));

  set = munge_items (view);

  /* TODO: make and use mojito_set_equal () */
  g_assert (mojito_set_size (set) == 1);
  g_assert (!mojito_set_has (set, G_OBJECT (old_item)));
  g_assert (mojito_set_has (set, G_OBJECT (new_item)));

  g_object_unref (view);
#endif
}

/*
 * Create a view of 2 items on a two services, add 2 items from each to the pending
 * list, and check that the latest item from each source was selected.
 */
void
test_view_munge_3 (void)
{
  g_message ("This test is disabled");
#if 0
  MojitoView *view;
  MojitoViewPrivate *priv;
  MojitoService *service_1, *service_2;
  MojitoItem *item_1, *item_2;
  MojitoSet *set;

  view = mojito_view_new (2);
  priv = view->priv;

  service_1 = g_object_new (MOJITO_TYPE_SERVICE_DUMMY, NULL);
  mojito_view_add_service (view, service_1, NULL);
  service_2 = g_object_new (MOJITO_TYPE_SERVICE_DUMMY, NULL);
  mojito_view_add_service (view, service_2, NULL);

  mojito_set_add (priv->pending_items, G_OBJECT (make_item_dated (service_1, 1)));
  item_1 = make_item_dated (service_1, 1000);
  mojito_set_add (priv->pending_items, G_OBJECT (item_1));

  mojito_set_add (priv->pending_items, G_OBJECT (make_item_dated (service_2, 1)));
  item_2 = make_item_dated (service_2, 1000);
  mojito_set_add (priv->pending_items, G_OBJECT (item_2));

  set = munge_items (view);

  /* TODO: make and use mojito_set_equal () */
  g_assert (mojito_set_size (set) == 2);
  g_assert (mojito_set_has (set, G_OBJECT (item_1)));
  g_assert (mojito_set_has (set, G_OBJECT (item_2)));

  g_object_unref (view);
#endif
}

/*
 * Create a view of 3 items on a two services, add 3 items from one and none
 * from the other, and check that all of the items were included.
 */
void
test_view_munge_4 (void)
{
  g_message ("This test is disabled");
#if 0
  MojitoView *view;
  MojitoViewPrivate *priv;
  MojitoService *service_1, *service_2;
  GList *items = NULL;
  MojitoSet *set;
  int i;

  view = mojito_view_new (3);
  priv = view->priv;

  service_1 = g_object_new (MOJITO_TYPE_SERVICE_DUMMY, NULL);
  mojito_view_add_service (view, service_1, NULL);
  service_2 = g_object_new (MOJITO_TYPE_SERVICE_DUMMY, NULL);
  mojito_view_add_service (view, service_2, NULL);

  for (i = 0; i < 3; i++) {
    MojitoItem *item;
    item = make_item (service_1);
    items = g_list_prepend (items, item);
    mojito_set_add (priv->pending_items, G_OBJECT (item));
  }

  set = munge_items (view);

  g_assert (mojito_set_size (set) == 3);

  while (items) {
    g_assert (mojito_set_has (set, items->data));
    items = items->next;
  }

  g_object_unref (view);
#endif
}

#endif
