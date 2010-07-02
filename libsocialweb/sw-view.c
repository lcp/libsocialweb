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

#include <math.h>
#include "sw-view.h"

#include <libsocialweb/sw-core.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-cache.h>
#include <libsocialweb/sw-online.h>
#include <libsocialweb/sw-item-view.h>

G_DEFINE_TYPE (SwView, sw_view, SW_TYPE_ITEM_VIEW);

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_VIEW, SwViewPrivate))

/* Refresh every 5 minutes */
#define REFRESH_TIMEOUT (5 * 60)

/* Wait 200ms between queued recalculates before recalculating */
#define RECALCULATE_DELAY 200

struct _SwViewPrivate {
  SwCore *core;
  /* List of SwService objects */
  GList *services;
  /* The maximum number of items in the view */
  guint count;
  /* The refresh timeout id */
  guint refresh_timeout_id;
  /* The set of all items currently being considered for the client */
  SwSet *all_items;
  /* Whether we're currently running */
  gboolean running;
  /* For recalculate queue */
  guint recalculate_timeout_id;
  /* The last time we recalculated. Use for changed items. */
  time_t last_recalculate_time;
};

enum {
  PROP_0,
  PROP_CORE,
  PROP_COUNT,
};

static int
get_service_count (GHashTable *hash,
                   SwService  *service)
{
  return GPOINTER_TO_INT (g_hash_table_lookup (hash, service));
}

static void
inc_service_count (GHashTable *hash,
                   SwService  *service)
{
  int count;
  count = GPOINTER_TO_INT (g_hash_table_lookup (hash, service));
  ++count;
  g_hash_table_insert (hash, service, GINT_TO_POINTER (count));
}

static SwSet *
munge_items (SwView *view)
{
  SwViewPrivate *priv = view->priv;
  GList *list, *l = NULL;
  int count, service_max;
  GHashTable *counts;
  SwSet *new;
  SwItem *item;

  /* The magic */
  list = sw_set_as_list (priv->all_items);

  while (list) {
    item = (SwItem *)list->data;
    if (!sw_core_is_item_banned (priv->core, item)) {
      if (sw_item_get_ready (item)) {
        l = g_list_prepend (l, item);
        SW_DEBUG (VIEWS, "Item ready: %s",
                      sw_item_get (item, "id"));
      } else {
        SW_DEBUG (VIEWS, "Item not ready. Skipping: %s",
                      sw_item_get (item, "id"));
      }
    }
    list = g_list_delete_link (list, list);
  }
  list = l;

  list = g_list_sort (list, (GCompareFunc)sw_item_compare_date_newer);

  counts = g_hash_table_new (NULL, NULL);
  service_max = ceil ((float)priv->count / g_list_length (priv->services));

  count = 0;
  new = sw_item_set_new ();

  /* We manipulate list in place instead of using a temporary GList* because
   * need to keep track of the new head.  This loop exits if we have ran out of
   * items from the sources (list == NULL), we have filled up the result list
   * (count >= priv->count), or if we have reached the end of the list but have
   * not yet filled up the result list.
  */
  /* TODO: use gsequence instead for clarity? */
  while (list && count < priv->count) {
    SwItem *item;
    SwService *service;

    item = list->data;
    service = sw_item_get_service (item);

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
    sw_set_add (new, (GObject*)item);
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
    sw_set_add (new, item);
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
remove_service (GObject  *object,
                gpointer  user_data)
{
  SwItem *item = SW_ITEM (object);
  SwService *service = SW_SERVICE (user_data);

  return sw_item_get_service (item) == service;
}

void
sw_view_recalculate (SwView *view)
{
  SwViewPrivate *priv;
  SwSet *new_items;

  g_return_if_fail (SW_IS_VIEW (view));

  priv = view->priv;
  if (!priv->running)
    return;

  new_items = munge_items (view);

  sw_item_view_set_from_set (SW_ITEM_VIEW (view), new_items);
}

static void
service_refreshed_cb (SwService *service,
                      SwSet     *set,
                      gpointer   user_data)
{
  SwView *view = SW_VIEW (user_data);
  SwViewPrivate *priv = view->priv;
  GHashTable *params;

  g_object_get (service, "params", &params, NULL);

  if (set) {
    sw_cache_save (service, NULL, params, set);
  } else {
    sw_cache_drop (service, NULL, params);
  }

  g_hash_table_unref (params);

  /* Remove all existing items from this service */
  sw_set_foreach_remove (priv->all_items, remove_service, service);
  if (set) {
    sw_set_add_from (priv->all_items, set);
  }

  sw_view_recalculate (view);
}

/*
 * Called in a timeout and asks all of the sources to refresh themselves.
 */
static gboolean
start_refresh (SwView *view)
{
  SwViewPrivate *priv = view->priv;
  GList *l;

  for (l = priv->services; l; l = l->next) {
    SwService *service = l->data;
    SW_DEBUG (VIEWS, "Refreshing %s on view %p",
              sw_service_get_name (service),
              view);
    sw_service_refresh (service);
  }

  return TRUE;
}

static void
load_cache (SwView *view)
{
  SwViewPrivate *priv = view->priv;
  GList *l;

  for (l = priv->services; l; l = l->next) {
    SwService *service = l->data;
    SwSet *set;
    GHashTable *params;

    g_object_get (service, "params", &params, NULL);
    set = sw_cache_load (service, NULL, params);
    g_hash_table_unref (params);

    service_refreshed_cb (service, set, view);

    if (set)
      sw_set_unref (set);
  }
}

static void
install_refresh_timeout (SwView *view)
{
  if (view->priv->refresh_timeout_id == 0)
    view->priv->refresh_timeout_id = g_timeout_add_seconds (REFRESH_TIMEOUT,
                                                            (GSourceFunc)start_refresh,
                                                            view);
}

static void
remove_refresh_timeout (SwView *view)
{
  if (view->priv->refresh_timeout_id) {
    g_source_remove (view->priv->refresh_timeout_id);
    view->priv->refresh_timeout_id = 0;
  }
}

static void
online_notify (gboolean online,
               gpointer user_data)
{
  SwView *view = user_data;

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
view_start (SwItemView *item_view)
{
  SwView *view = (SwView *)item_view;
  GList *l;

  view->priv->running = TRUE;

  /* Tell the services to start */
  for (l = view->priv->services; l; l = l->next) {
    SwService *service = l->data;
    sw_service_start (service);
  }

  if (sw_is_online ()) {
    online_notify (TRUE, view);
  } else {
    online_notify (FALSE, view);
    /* Online notify doesn't load the cache if we are offline, so do that now */
    load_cache (view);
  }
}

static void
view_refresh (SwItemView *item_view)
{
  SwView *view = (SwView *)item_view;

  /* Reinstall the timeout */
  remove_refresh_timeout (view);
  install_refresh_timeout (view);

  start_refresh (view);
}

static void
sw_view_get_property (GObject    *object,
                      guint       property_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  SwViewPrivate *priv = SW_VIEW (object)->priv;

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
sw_view_set_property (GObject      *object,
                      guint         property_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  SwViewPrivate *priv = SW_VIEW (object)->priv;

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
sw_view_dispose (GObject *object)
{
  SwView *view = SW_VIEW (object);
  SwViewPrivate *priv = view->priv;

  if (priv->recalculate_timeout_id)
  {
    g_source_remove (priv->recalculate_timeout_id);
    priv->recalculate_timeout_id = 0;
  }

  if (priv->core) {
    g_object_unref (priv->core);
    priv->core = NULL;
  }

  if (priv->all_items) {
    sw_set_unref (priv->all_items);
    priv->all_items = NULL;
  }

  while (priv->services) {
    SwService *service = priv->services->data;

    g_signal_handlers_disconnect_by_func (service, service_refreshed_cb, object);
    g_object_unref (service);
    priv->services = g_list_delete_link (priv->services, priv->services);
  }

  G_OBJECT_CLASS (sw_view_parent_class)->dispose (object);
}


static void
sw_view_finalize (GObject *object)
{
  SwView *view = SW_VIEW (object);

  sw_online_remove_notify (online_notify, view);

  G_OBJECT_CLASS (sw_view_parent_class)->finalize (object);
}

static void
sw_view_class_init (SwViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwItemViewClass *item_view_class = SW_ITEM_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwViewPrivate));

  object_class->get_property = sw_view_get_property;
  object_class->set_property = sw_view_set_property;
  object_class->dispose = sw_view_dispose;
  object_class->finalize = sw_view_finalize;

  item_view_class->start = view_start;
  item_view_class->refresh = view_refresh;

  pspec = g_param_spec_object ("core", "core", "The core",
                               SW_TYPE_CORE,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_CORE, pspec);

  pspec = g_param_spec_uint ("count", "count", "The number of items",
                             1, G_MAXUINT, 10,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_COUNT, pspec);
}

static void
sw_view_init (SwView *self)
{
  self->priv = GET_PRIVATE (self);

  self->priv->all_items = sw_item_set_new ();

  self->priv->running = FALSE;

  sw_online_add_notify (online_notify, self);
}

SwView*
sw_view_new (SwCore *core, guint count)
{
  return g_object_new (SW_TYPE_VIEW,
                       "core", core,
                       "count", count,
                       NULL);
}

/* This adopts the reference passed */
void
sw_view_add_service (SwView     *view,
                     SwService  *service,
                     GHashTable *params)
{
  SwViewPrivate *priv = view->priv;

  priv->services = g_list_append (priv->services, service);
  /* TODO: use _object etc */
  /* We disconect this in the dispose */
  g_signal_connect (service,
                    "refreshed",
                    G_CALLBACK (service_refreshed_cb),
                    view);
}


#if BUILD_TESTS

#include <services/dummy/dummy.h>

void
test_view_new (void)
{
  SwView *view;
  view = sw_view_new (NULL, 5);
  g_object_unref (view);
}

#if 0
static SwItem *
make_item_dated (SwService *service, time_t date)
{
  SwItem *item;
  char *id;

  g_assert (service);

  id = g_strdup_printf ("random-item-%d", g_test_rand_int_range (0, G_MAXINT));

  item = sw_item_new ();
  sw_item_set_service (item, service);
  sw_item_put (item, "id", id);
  sw_item_take (item, "title", id);
  sw_item_put (item, "date", sw_time_t_to_string (date));

  return item;
}

static SwItem *
make_item (SwService *service)
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
  SwView *view;
  SwViewPrivate *priv;
  SwService *service;
  GList *items = NULL;
  SwSet *set;
  int i;

  view = sw_view_new (5);
  priv = view->priv;

  service = g_object_new (SW_TYPE_SERVICE_DUMMY, NULL);
  sw_view_add_service (view, service, NULL);

  for (i = 0; i < 5; i++) {
    SwItem *item;
    item = make_item (service);
    items = g_list_prepend (items, item);
    sw_set_add (priv->pending_items, G_OBJECT (item));
  }

  set = munge_items (view);

  g_assert (sw_set_size (set) == 5);

  while (items) {
    g_assert (sw_set_has (set, items->data));
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
  SwView *view;
  SwViewPrivate *priv;
  SwService *service;
  SwItem *old_item, *new_item;
  SwSet *set;

  view = sw_view_new (1);
  priv = view->priv;

  service = g_object_new (SW_TYPE_SERVICE_DUMMY, NULL);
  sw_view_add_service (view, service, NULL);

  old_item = make_item_dated (service, 1);
  sw_set_add (priv->pending_items, G_OBJECT (old_item));

  new_item = make_item_dated (service, 1000);
  sw_set_add (priv->pending_items, G_OBJECT (new_item));

  set = munge_items (view);

  /* TODO: make and use sw_set_equal () */
  g_assert (sw_set_size (set) == 1);
  g_assert (!sw_set_has (set, G_OBJECT (old_item)));
  g_assert (sw_set_has (set, G_OBJECT (new_item)));

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
  SwView *view;
  SwViewPrivate *priv;
  SwService *service_1, *service_2;
  SwItem *item_1, *item_2;
  SwSet *set;

  view = sw_view_new (2);
  priv = view->priv;

  service_1 = g_object_new (SW_TYPE_SERVICE_DUMMY, NULL);
  sw_view_add_service (view, service_1, NULL);
  service_2 = g_object_new (SW_TYPE_SERVICE_DUMMY, NULL);
  sw_view_add_service (view, service_2, NULL);

  sw_set_add (priv->pending_items, G_OBJECT (make_item_dated (service_1, 1)));
  item_1 = make_item_dated (service_1, 1000);
  sw_set_add (priv->pending_items, G_OBJECT (item_1));

  sw_set_add (priv->pending_items, G_OBJECT (make_item_dated (service_2, 1)));
  item_2 = make_item_dated (service_2, 1000);
  sw_set_add (priv->pending_items, G_OBJECT (item_2));

  set = munge_items (view);

  /* TODO: make and use sw_set_equal () */
  g_assert (sw_set_size (set) == 2);
  g_assert (sw_set_has (set, G_OBJECT (item_1)));
  g_assert (sw_set_has (set, G_OBJECT (item_2)));

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
  SwView *view;
  SwViewPrivate *priv;
  SwService *service_1, *service_2;
  GList *items = NULL;
  SwSet *set;
  int i;

  view = sw_view_new (3);
  priv = view->priv;

  service_1 = g_object_new (SW_TYPE_SERVICE_DUMMY, NULL);
  sw_view_add_service (view, service_1, NULL);
  service_2 = g_object_new (SW_TYPE_SERVICE_DUMMY, NULL);
  sw_view_add_service (view, service_2, NULL);

  for (i = 0; i < 3; i++) {
    SwItem *item;
    item = make_item (service_1);
    items = g_list_prepend (items, item);
    sw_set_add (priv->pending_items, G_OBJECT (item));
  }

  set = munge_items (view);

  g_assert (sw_set_size (set) == 3);

  while (items) {
    g_assert (sw_set_has (set, items->data));
    items = items->next;
  }

  g_object_unref (view);
#endif
}

#endif
