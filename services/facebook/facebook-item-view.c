/*
 * libsocialweb Facebook service support
 *
 * Copyright (C) 2010 Novell, Inc.
 * Copyright (C) Collabora Ltd.
 *
 * Authors: Gary Ching-Pang Lin <glin@novell.com>
 *          Thomas Thurman <thomas.thurman@collabora.co.uk>
 *          Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *          Danielle Madeley <danielle.madeley@collabora.co.uk>
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

#include "facebook-item-view.h"
#include "facebook.h"
#include "facebook-util.h"

#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-cache.h>

#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <json-glib/json-glib.h>

#define GET_PRIVATE(o) (((SwFacebookItemView *) o)->priv)

#define UPDATE_TIMEOUT (5*60)

G_DEFINE_TYPE (SwFacebookItemView, sw_facebook_item_view, SW_TYPE_ITEM_VIEW);

static void facebook_item_view_stop (SwItemView *self);
static void facebook_item_view_refresh (SwItemView *self);

struct _SwFacebookItemViewPrivate
{
  RestProxy *proxy;
  gchar *query;
  GHashTable *params;

  guint running;
};

enum /* properties */
{
  PROP_0,
  PROP_PROXY,
  PROP_QUERY,
  PROP_PARAMS
};

static char*
_facebook_status_node_get_link (JsonNode *status_node)
{
  JsonObject *status_object = json_node_get_object (status_node);
  char *url = get_child_node_value (status_node, "link");

  if (url == NULL)
    {
      /* try to extract a link to the 'comment' action for this post, which
       * serves as a 'permalink' for a particular status update */
      JsonArray *actions = NULL;
      JsonNode *actions_node = json_object_get_member (status_object,
                                                       "actions");

      if (actions_node != NULL)
        actions = json_node_get_array (actions_node);

      if (actions != NULL)
        {
          guint j;

          for (j = 0; j < json_array_get_length (actions); j++)
            {
              JsonNode *action = json_array_get_element (actions, j);
              char *action_name;

              action_name = get_child_node_value (action, "name");

              if (action_name == NULL)
                {
                  continue;
                }
              else if (g_ascii_strcasecmp (action_name, "Comment") != 0)
                {
                  g_free (action_name);

                  continue;
                }

              g_free (action_name);

              url = get_child_node_value (action, "link");

              break;
            }
          }
    }

  if (url == NULL)
    {
      /* can't find a decent url to associate with this post, so just link to
       * the facebook homepage */
      url = g_strdup ("http://www.facebook.com");
    }

  return url;
}

static SwItem*
_facebook_status_node_to_item (SwItemView *self,
                               JsonNode   *status_node)
{
  SwFacebookItemViewPrivate *priv = GET_PRIVATE (self);
  SwItem *item;
  char *id, *uid, *post_time, *message, *pic_url;
  char *name = NULL, *authorid = NULL;
  char *thumbnail = NULL;
  char *url = NULL;
  char *post_type = NULL;
  const char *my_uid = sw_service_facebook_get_uid (
      (SwServiceFacebook *) sw_item_view_get_service (self));
  JsonObject *status_object;
  JsonNode *from, *to;

  if (json_node_get_node_type (status_node) != JSON_NODE_OBJECT)
    return NULL;

  status_object = json_node_get_object (status_node);

  /* We only support status messages at the moment.  In the future, we may add
   * support for photos, links, etc */
  post_type = get_child_node_value (status_node, "type");
  if (g_strcmp0 (post_type, "status") != 0)
    {
      g_free (post_type);

      return NULL;
    }

  g_free (post_type);

  /* In addition, we don't yet support messages that are targetted at a
   * particular user (unless that user is you).  Without any context about whose
   * wall the message was written on, these user-to-user messages become rather
   * confusing. So before we support them, we would need to introduce a concept
   * of a 'target' user to SwItem or something similar */
  to = json_object_get_member (status_object, "to");

  if (to != NULL)
    {
      JsonObject *to_obj = NULL;
      JsonArray *to_array = NULL;
      guint i;
      gboolean to_me = FALSE;

      to_obj = json_node_get_object (to);
      to_array = json_object_get_array_member (to_obj, "data");

      for (i = 0; i < json_array_get_length (to_array); i++)
        {
          JsonNode *user;

          user = json_array_get_element (to_array, i);

          if (user != NULL)
            {
              char *to_id = get_child_node_value (user, "id");

              if (to_id != NULL && g_strcmp0 (my_uid, to_id) == 0)
                {
                  to_me = TRUE;

                  g_free (to_id);

                  break;
                }

              g_free (to_id);
            }
        }

      if (!to_me)
        return NULL;
    }

  item = sw_item_new ();
  sw_item_set_service (item, sw_item_view_get_service (self));

  /* we use created_time here so that items don't keep getting pushed up to
   * the top of the list when people comment on them, etc.  If and when we
   * implement support for comments on items, we could revisit that decision
   */
  post_time = get_child_node_value (status_node, "created_time");
  if (post_time == NULL)
    {
      g_debug ("Got status update without a date");
      g_object_unref (item);

      return NULL;
    }

  sw_item_take (item, "date", post_time);

  /* Construct item ID */
  uid = get_child_node_value (status_node, "id");
  if (uid == NULL)
    {
      g_debug ("Got status update without an id");
      g_object_unref (item);

      return NULL;
    }

  id = g_strconcat ("facebook-", uid, NULL);
  g_free (uid);
  sw_item_take (item, "id", id);

  message = get_child_node_value (status_node, "message");
  if (message == NULL || message[0] == '\0')
    {
      g_debug ("Got status update without a message");
      g_free (message);
      g_object_unref (item);

      return NULL;
  }
  sw_item_take (item, "content", message);

  from = json_object_get_member (status_object, "from");
  if (from != NULL)
    {
      name = get_child_node_value (from, "name");
      authorid = get_child_node_value (from, "id");
      sw_item_take (item, "authorid", authorid);
    }

  if (name == NULL)
    {
      g_debug ("Got status update without an author name");
      g_object_unref (item);

      return NULL;
  }
  sw_item_take (item, "author", name);

  if (authorid != NULL)
    {
      pic_url = build_picture_url (priv->proxy, authorid,
                                   FB_PICTURE_SIZE_SQUARE);
      sw_item_request_image_fetch (item, FALSE, "authoricon", pic_url);
      g_free (pic_url);
    }

  /* thumbnail is not likely to exist on a status update, but just in case */
  thumbnail = get_child_node_value (status_node, "picture");
  if (thumbnail != NULL)
    {
      sw_item_request_image_fetch (item, FALSE, "thumbnail", thumbnail);
      g_free (thumbnail);
    }

  url = _facebook_status_node_get_link (status_node);
  if (url != NULL)
    sw_item_take (item, "url", url);

  return item;
}

static SwSet*
_facebook_status_node_to_set (SwItemView *self,
                              JsonNode   *root)
{
  JsonObject *root_object = NULL;
  JsonNode *statuses = NULL;
  JsonArray *status_array = NULL;
  SwSet *set = NULL;
  guint i;

  if (json_node_get_node_type (root) == JSON_NODE_OBJECT)
    root_object = json_node_get_object (root);
  else
    return NULL;

  if (!json_object_has_member (root_object, "data"))
    return NULL;

  statuses = json_object_get_member (root_object, "data");

  if (json_node_get_node_type (statuses) == JSON_NODE_ARRAY)
    status_array = json_node_get_array (statuses);
  else
    return NULL;

  set = sw_item_set_new ();

  for (i = 0; i < json_array_get_length (status_array); i++)
    {
      JsonNode *status;
      status = json_array_get_element (status_array, i);

      SwItem *item = _facebook_status_node_to_item (self, status);

      if (item != NULL)
        {
          sw_set_add (set, G_OBJECT (item));
          g_object_unref (item);
        }
    }

  return set;
}

static void
got_status_cb (RestProxyCall *call,
               GError        *error,
               GObject       *weak_object,
               gpointer       userdata)
{
  SwItemView *self = SW_ITEM_VIEW (weak_object);
  SwFacebookItemViewPrivate *priv = GET_PRIVATE (self);
  JsonNode *root;
  SwSet *set;

  if (error)
    {
      g_message ("Error: %s", error->message);

      return;
    }

  root = json_node_from_call (call, NULL);
  if (!root)
    return;

  set = _facebook_status_node_to_set (self, root);

  json_node_free (root);

  if (set != NULL)
    {
      sw_item_view_set_from_set (self, set);

      /* Save the results of this set to the cache */
      sw_cache_save (sw_item_view_get_service (self),
                     priv->query,
                     priv->params,
                     set);

      sw_set_unref (set);
    }
}

static void
get_status_updates (SwItemView *self)
{
  SwFacebookItemViewPrivate *priv = GET_PRIVATE (self);
  RestProxyCall *call;
  const char *my_uid = sw_service_facebook_get_uid (
      (SwServiceFacebook *) sw_item_view_get_service (self));

  if (my_uid == NULL || priv->running == 0)
    return;

  call = rest_proxy_new_call (priv->proxy);

  if (g_strcmp0 (priv->query, "own") == 0)
    rest_proxy_call_set_function (call, "me/feed");
  else if (g_strcmp0 (priv->query, "feed") == 0 ||
           g_strcmp0 (priv->query, "friends-only") == 0)
    rest_proxy_call_set_function (call, "me/home");
  else
    g_return_if_reached ();

  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback) got_status_cb,
                         (GObject*) self,
                         NULL,
                         NULL);

  g_object_unref (call);
}

static void
load_from_cache (SwItemView *self)
{
  SwFacebookItemViewPrivate *priv = GET_PRIVATE (self);
  SwSet *set;

  set = sw_cache_load (sw_item_view_get_service (self),
                       priv->query,
                       priv->params);

  if (set != NULL)
    {
      sw_item_view_set_from_set (self, set);

      sw_set_unref (set);
    }
}

static void
_service_item_hidden (SwService   *service,
                      const gchar *uid,
                      SwItemView  *self)
{
  sw_item_view_remove_by_uid (self, uid);
}

static void
_service_user_changed (SwService  *service,
                       SwItemView *self)
{
  SwSet *set;

  /* We need to empty the set */
  set = sw_item_set_new ();
  sw_item_view_set_from_set (self, set);
  sw_set_unref (set);

  /* And drop the cache */
  sw_cache_drop_all (service);
}

static void
_service_capabilities_changed (SwService    *service,
                               const gchar **caps,
                               SwItemView   *self)
{
  if (sw_service_has_cap (caps, CREDENTIALS_VALID))
  {
    facebook_item_view_refresh (self);
  }
}

static void
facebook_item_view_constructed (GObject *self)
{
  SwService *service = sw_item_view_get_service ((SwItemView *) self);

  g_signal_connect_object (service, "item-hidden",
                    G_CALLBACK (_service_item_hidden), self, 0);
  g_signal_connect_object (service, "user-changed",
                    G_CALLBACK (_service_user_changed), self, 0);
  g_signal_connect_object (service, "capabilities-changed",
                    G_CALLBACK (_service_capabilities_changed), self, 0);

  if (G_OBJECT_CLASS (sw_facebook_item_view_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (sw_facebook_item_view_parent_class)->constructed (self);
}

static void
facebook_item_view_set_property (GObject      *self,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  SwFacebookItemViewPrivate *priv = GET_PRIVATE (self);

  switch (property_id)
    {
      case PROP_PROXY:
        priv->proxy = g_value_dup_object (value);
        break;

      case PROP_QUERY:
        priv->query = g_value_dup_string (value);
        break;

      case PROP_PARAMS:
        priv->params = g_value_dup_boxed (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
        break;
    }
}

static void
facebook_item_view_get_property (GObject    *self,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  SwFacebookItemViewPrivate *priv = GET_PRIVATE (self);

  switch (property_id)
    {
      case PROP_PROXY:
        g_value_set_object (value, priv->proxy);
        break;

      case PROP_QUERY:
        g_value_set_string (value, priv->query);
        break;

      case PROP_PARAMS:
        g_value_set_boxed (value, priv->params);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
        break;
    }
}

static void
facebook_item_view_dispose (GObject *self)
{
  SwFacebookItemViewPrivate *priv = GET_PRIVATE (self);

  facebook_item_view_stop ((SwItemView *) self);

  g_object_unref (priv->proxy);
  priv->proxy = NULL;

  G_OBJECT_CLASS (sw_facebook_item_view_parent_class)->dispose (self);
}

static void
facebook_item_view_finalize (GObject *self)
{
  SwFacebookItemViewPrivate *priv = GET_PRIVATE (self);

  g_free (priv->query);
  g_boxed_free (G_TYPE_HASH_TABLE, priv->params);

  G_OBJECT_CLASS (sw_facebook_item_view_parent_class)->finalize (self);
}

static gboolean
_update_timeout_cb (gpointer user_data)
{
   get_status_updates ((SwItemView *) user_data);

   return TRUE;
}

static void
facebook_item_view_start (SwItemView *self)
{
  SwFacebookItemViewPrivate *priv = GET_PRIVATE (self);

  if (priv->running != 0)
    {
      g_message (G_STRLOC ": View asked to start, but already running");
    }
  else
    {
      g_debug ("Starting up the Facebook view");

      priv->running = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                             _update_timeout_cb,
                                             self);

      load_from_cache (self);
      get_status_updates (self);
    }
}

static void
facebook_item_view_stop (SwItemView *self)
{
  SwFacebookItemViewPrivate *priv = GET_PRIVATE (self);

  if (priv->running == 0)
    {
      g_message (G_STRLOC ": View ask to stop, but not running");
    }
  else
    {
      g_debug ("Stopping the Facebook view");

      g_source_remove (priv->running);
      priv->running = 0;
    }

}

static void
facebook_item_view_refresh (SwItemView *self)
{
  g_debug ("Forced a refresh of the Facebook view");

  get_status_updates (self);
}

static void
sw_facebook_item_view_class_init (SwFacebookItemViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  SwItemViewClass *item_view_class = SW_ITEM_VIEW_CLASS (klass);

  gobject_class->constructed = facebook_item_view_constructed;
  gobject_class->set_property = facebook_item_view_set_property;
  gobject_class->get_property = facebook_item_view_get_property;
  gobject_class->dispose = facebook_item_view_dispose;
  gobject_class->finalize = facebook_item_view_finalize;

  item_view_class->start = facebook_item_view_start;
  item_view_class->stop = facebook_item_view_stop;
  item_view_class->refresh = facebook_item_view_refresh;

  g_object_class_install_property (gobject_class, PROP_PROXY,
      g_param_spec_object ("proxy",
        "Proxy",
        "The #RestProxy we're using to make API calls",
        REST_TYPE_PROXY,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_QUERY,
      g_param_spec_string ("query",
        "Query",
        "The query requested for this view",
        NULL,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PARAMS,
      g_param_spec_boxed ("params",
        "Params",
        "Additional parameters passed to the query",
        G_TYPE_HASH_TABLE,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (gobject_class, sizeof (SwFacebookItemViewPrivate));
}

static void
sw_facebook_item_view_init (SwFacebookItemView *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self),
      SW_TYPE_FACEBOOK_ITEM_VIEW, SwFacebookItemViewPrivate);
}
