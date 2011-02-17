/*
 * libsocialweb MySpace service support
 * Copyright (C) 2008 - 2009 Intel Corporation.
 * Copyright (C) 2010 Novell, Inc.
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

#include <time.h>
#include <stdlib.h>
#include <string.h>

#include <rest/rest-proxy.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-cache.h>

#include "myspace-item-view.h"
#include "myspace.h"

G_DEFINE_TYPE (SwMySpaceItemView,
               sw_myspace_item_view,
               SW_TYPE_ITEM_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_MYSPACE_ITEM_VIEW, SwMySpaceItemViewPrivate))

typedef struct _SwMySpaceItemViewPrivate SwMySpaceItemViewPrivate;

struct _SwMySpaceItemViewPrivate {
  RestProxy *proxy;
  guint timeout_id;
  GHashTable *params;
  gchar *query;
};

enum
{
  PROP_0,
  PROP_PROXY,
  PROP_PARAMS,
  PROP_QUERY
};

#define UPDATE_TIMEOUT 5 * 60

static void _service_item_hidden_cb (SwService   *service,
                                     const gchar *uid,
                                     SwItemView  *item_view);
static void _service_user_changed_cb (SwService  *service,
                                      SwItemView *item_view);
static void _service_capabilities_changed_cb (SwService    *service,
                                              const gchar **caps,
                                              SwItemView   *item_view);

static void
sw_myspace_item_view_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      g_value_set_object (value, priv->proxy);
      break;
    case PROP_PARAMS:
      g_value_set_boxed (value, priv->params);
      break;
    case PROP_QUERY:
      g_value_set_string (value, priv->query);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_myspace_item_view_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      if (priv->proxy)
      {
        g_object_unref (priv->proxy);
      }
      priv->proxy = g_value_dup_object (value);
      break;
    case PROP_PARAMS:
      priv->params = g_value_dup_boxed (value);
      break;
    case PROP_QUERY:
      priv->query = g_value_dup_string (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_myspace_item_view_dispose (GObject *object)
{
  SwItemView *item_view = SW_ITEM_VIEW (object);
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (object);

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->timeout_id) {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }

  g_signal_handlers_disconnect_by_func (sw_item_view_get_service (item_view),
                                        _service_item_hidden_cb,
                                        item_view);
  g_signal_handlers_disconnect_by_func (sw_item_view_get_service (item_view),
                                        _service_user_changed_cb,
                                        item_view);
  g_signal_handlers_disconnect_by_func (sw_item_view_get_service (item_view),
                                        _service_capabilities_changed_cb,
                                        item_view);

  G_OBJECT_CLASS (sw_myspace_item_view_parent_class)->dispose (object);
}

static void
sw_myspace_item_view_finalize (GObject *object)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (object);

  /* free private variables */
  g_free (priv->query);
  g_hash_table_unref (priv->params);

  G_OBJECT_CLASS (sw_myspace_item_view_parent_class)->finalize (object);
}

static JsonNode *
json_node_from_call (RestProxyCall *call,
                     const char    *name)
{
  JsonParser *parser;
  JsonNode *root = NULL;
  GError *error;
  gboolean ret = FALSE;

  parser = json_parser_new ();

  if (call == NULL)
    goto out;

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_message ("Error from %s: %s (%d)",
               name,
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    goto out;
  }

  ret = json_parser_load_from_data (parser,
                                    rest_proxy_call_get_payload (call),
                                    rest_proxy_call_get_payload_length (call),
                                    &error);
  root = json_parser_get_root (parser);

  if (root == NULL) {
    g_message ("Error from %s: %s",
               name,
               rest_proxy_call_get_payload (call));
    goto out;
  }

  root = json_node_copy (root);

out:
  g_object_unref (parser);

  return root;
}

static char *
make_date (const char *s)
{
  struct tm tm;

  /* Time format example: 2010-12-07T10:02:22Z */
  strptime (s, "%FT%T%z", &tm);
  return sw_time_t_to_string (timegm (&tm));
}

static SwItem *
make_item (SwService *service, JsonNode *entry)
{
  SwItem *item;
  JsonNode *author;
  JsonObject *entry_obj, *author_obj;
  const char *tmp;
  char *status;

  item = sw_item_new ();
  sw_item_set_service (item, service);

  entry_obj = json_node_get_object (entry);
  author = json_object_get_member (entry_obj, "author");
  author_obj = json_node_get_object (author);

  /*
    id: myspace-<statusId>
    authorid: <userId>
    author: <displayName>
    authoricon: <thumbnailUrl>
    content: <status>
    date: <moodStatusLastUpdated>
    url: <profileUrl>
  */

  /* Construct the id of sw_item */
  tmp = json_object_get_string_member (entry_obj, "statusId");
  sw_item_take (item, "id", g_strconcat ("myspace-", tmp, NULL));

  /* Get the user id for authorid */
  tmp = json_object_get_string_member (entry_obj, "userId");
  sw_item_put (item, "authorid", tmp);

  /* Get the user name */
  tmp = json_object_get_string_member (author_obj, "displayName");
  sw_item_put (item, "author", tmp);

  /* Get the url of avatar */
  tmp = json_object_get_string_member (author_obj, "thumbnailUrl");
  sw_item_request_image_fetch (item, FALSE, "authoricon", g_strdup (tmp));;

  /* Get the content */
  status = json_object_get_string_member (entry_obj, "status");
  sw_item_put (item, "content", sw_unescape_entities ((gchar *)status));
  /* TODO: if mood is not "(none)" then append that to the status message */

  /* Get the date */
  tmp = json_object_get_string_member (entry_obj, "moodStatusLastUpdated");
  sw_item_take (item, "date", make_date (tmp));

  /* Get the url of this status */
  /* TODO find out the true url instead of the profile url */
  tmp = json_object_get_string_member (author_obj, "profileUrl");
  sw_item_put (item, "url", tmp);

  return item;
}

static void
_populate_set_from_node (SwService *service,
                         SwSet     *set,
                         JsonNode  *root)
{
  JsonNode *entries;
  JsonArray *status_array;
  JsonObject *object;
  guint i, length;

  /*
  The data format:
  "entry":[
    {
      "author":{
        "displayName":"username",
        "id":"myspace.com.person.<id>",
        "msUserType":"RegularUser",
        "name":{
          "familyName":"family name",
          "givenName":"given name"
        },
        "profileUrl":"http://www.myspace.com/<id>",
        "thumbnailUrl":"url of avatar"
      },
      "moodName":"none",
      "moodStatusLastUpdated":"2010-12-07T10:02:22Z",
      "numComments":"0",
      "status":"whatever you said",
      "statusId":"<status id>",
      "userId":"myspace.com.person.<id>"
    },
    ...
  ],
  */
  object = json_node_get_object (root);
  entries = json_object_get_member (object, "entry");

  status_array = json_node_get_array (entries);
  length = json_array_get_length (status_array);

  for (i=0; i<length; i++) {
    JsonNode *entry = json_array_get_element (status_array, i);
    SwItem *item;

    item = make_item (service, entry);

    if (!sw_service_is_uid_banned (service, sw_item_get (item, "id"))) {
      sw_set_add (set, (GObject *)item);
    }

    g_object_unref (item);
  }
}

static void
_got_status_cb (RestProxyCall *call,
                const GError  *error,
                GObject       *weak_object,
                gpointer       userdata)
{
  SwMySpaceItemView *item_view = SW_MYSPACE_ITEM_VIEW (weak_object);
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwSet *set = (SwSet *)userdata;
  JsonNode *root;
  SwService *service;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));

  root = json_node_from_call (call, "MySpace");
  if (root == NULL)
    return;

  _populate_set_from_node (service, set, root);

  g_object_unref (call);

  sw_item_view_set_from_set (SW_ITEM_VIEW (item_view), set);

  /* Save the results of this set to the cache */
  sw_cache_save (service,
                 priv->query,
                 priv->params,
                 set);

  sw_set_unref (set);

  g_object_unref (root);
}

static void
_get_user_status_updates (SwMySpaceItemView *item_view,
                          SwSet             *set)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestProxyCall *call;

  call = rest_proxy_new_call (priv->proxy);

  rest_proxy_call_set_function (call, "1.0/statusmood/@me/@self/history");
  rest_proxy_call_add_params(call,
                             "count", "20",
                             "fields", "author",
                             NULL);

  rest_proxy_call_async (call, _got_status_cb, (GObject*)item_view, set, NULL);
}

static void
_get_friends_status_updates (SwMySpaceItemView *item_view,
                             SwSet             *set)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestProxyCall *call;

  call = rest_proxy_new_call (priv->proxy);

  rest_proxy_call_set_function (call, "1.0/statusmood/@me/@friends/history");
  rest_proxy_call_add_params(call,
                             "includeself", "true",
                             "count", "20",
                             "fields", "author",
                             NULL);

  rest_proxy_call_async (call, _got_status_cb, (GObject*)item_view, set, NULL);
}

static void
_get_status_updates (SwMySpaceItemView *item_view)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (item_view);
  GHashTable *params = NULL;
  SwSet *set;

  set = sw_item_set_new ();

  if (g_str_equal (priv->query, "own"))
    _get_user_status_updates (item_view, set);
  else if (g_str_equal (priv->query, "feed"))
    _get_friends_status_updates (item_view, set);
  else
    g_error (G_STRLOC ": Unexpected query '%s'", priv->query);

  if (params)
    g_hash_table_unref (params);
}

static gboolean
_update_timeout_cb (gpointer data)
{
  SwMySpaceItemView *item_view = SW_MYSPACE_ITEM_VIEW (data);

  _get_status_updates (item_view);

  return TRUE;
}

static void
_load_from_cache (SwMySpaceItemView *item_view)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwSet *set;

  set = sw_cache_load (sw_item_view_get_service (SW_ITEM_VIEW (item_view)),
                       priv->query,
                       priv->params);

  if (set)
  {
    sw_item_view_set_from_set (SW_ITEM_VIEW (item_view),
                               set);
    sw_set_unref (set);
  }
}

static void
myspace_item_view_start (SwItemView *item_view)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              item_view);
    _load_from_cache ((SwMySpaceItemView *)item_view);
    _get_status_updates ((SwMySpaceItemView *)item_view);
  }
}

static void
myspace_item_view_stop (SwItemView *item_view)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (!priv->timeout_id)
  {
    g_warning (G_STRLOC ": View not running");
  } else {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }
}

static void
myspace_item_view_refresh (SwItemView *item_view)
{
  _get_status_updates ((SwMySpaceItemView *)item_view);
}

static void
_service_item_hidden_cb (SwService   *service,
                         const gchar *uid,
                         SwItemView  *item_view)
{
  sw_item_view_remove_by_uid (item_view, uid);
}

static void
_service_user_changed_cb (SwService  *service,
                          SwItemView *item_view)
{
  SwSet *set;

  /* We need to empty the set */
  set = sw_item_set_new ();
  sw_item_view_set_from_set (SW_ITEM_VIEW (item_view),
                             set);
  sw_set_unref (set);

  /* And drop the cache */
  sw_cache_drop_all (service);
}

static void
_service_capabilities_changed_cb (SwService    *service,
                                  const gchar **caps,
                                  SwItemView   *item_view)
{
  SwMySpaceItemViewPrivate *priv = GET_PRIVATE ((SwMySpaceItemView*) item_view);

  if (sw_service_has_cap (caps, CREDENTIALS_VALID))
  {
    myspace_item_view_refresh (item_view);

    if (!priv->timeout_id)
    {
      priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                                (GSourceFunc)_update_timeout_cb,
                                                item_view);
    }
  } else {
    if (priv->timeout_id)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }
  }
}

static void
sw_myspace_item_view_constructed (GObject *object)
{
  SwItemView *item_view = SW_ITEM_VIEW (object);

  g_signal_connect (sw_item_view_get_service (item_view),
                    "item-hidden",
                    (GCallback)_service_item_hidden_cb,
                    item_view);

  g_signal_connect (sw_item_view_get_service (item_view),
                    "user-changed",
                    (GCallback)_service_user_changed_cb,
                    item_view);

  g_signal_connect (sw_item_view_get_service (item_view),
                    "capabilities-changed",
                    (GCallback)_service_capabilities_changed_cb,
                    item_view);

  if (G_OBJECT_CLASS (sw_myspace_item_view_parent_class)->constructed)
    G_OBJECT_CLASS (sw_myspace_item_view_parent_class)->constructed (object);
}

static void
sw_myspace_item_view_class_init (SwMySpaceItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwItemViewClass *item_view_class = SW_ITEM_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwMySpaceItemViewPrivate));

  object_class->get_property = sw_myspace_item_view_get_property;
  object_class->set_property = sw_myspace_item_view_set_property;
  object_class->dispose = sw_myspace_item_view_dispose;
  object_class->finalize = sw_myspace_item_view_finalize;
  object_class->constructed = sw_myspace_item_view_constructed;

  item_view_class->start = myspace_item_view_start;
  item_view_class->stop = myspace_item_view_stop;
  item_view_class->refresh = myspace_item_view_refresh;

  pspec = g_param_spec_object ("proxy",
                               "proxy",
                               "proxy",
                               REST_TYPE_PROXY,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PROXY, pspec);

  pspec = g_param_spec_string ("query",
                               "query",
                               "query",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_QUERY, pspec);

  pspec = g_param_spec_boxed ("params",
                              "params",
                              "params",
                              G_TYPE_HASH_TABLE,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PARAMS, pspec);
}

static void
sw_myspace_item_view_init (SwMySpaceItemView *self)
{
}
