/*
 * libsocialweb Facebook service support
 *
 * Copyright (C) 2010 Novell, Inc.
 * Copyright (C) 2010-2011 Collabora Ltd.
 *
 * Authors: Gary Ching-Pang Lin <glin@novell.com>
 *          Thomas Thurman <thomas.thurman@collabora.co.uk>
 *          Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *          Danielle Madeley <danielle.madeley@collabora.co.uk>
 *          Marco Barisione <marco.barisione@collabora.co.uk>
 *          Alban Crequy <alban.crequy@collabora.co.uk>
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

#include "facebook-contact-view.h"
#include "facebook.h"
#include "facebook-util.h"

#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-contact.h>
#include <libsocialweb/sw-set.h>
#include <libsocialweb/sw-cache.h>

#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <json-glib/json-glib.h>

#define GET_PRIVATE(o) (((SwFacebookContactView *) o)->priv)

#define UPDATE_TIMEOUT (5*60)

G_DEFINE_TYPE (SwFacebookContactView, sw_facebook_contact_view, SW_TYPE_CONTACT_VIEW);

static void facebook_contact_view_stop (SwContactView *self);
static void facebook_contact_view_refresh (SwContactView *self);

struct _SwFacebookContactViewPrivate
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

static void
update_contact_from_node (SwContact      *dest_contact,
                       const gchar *dest_name,
                       JsonNode    *src_node,
                       const gchar *src_name)
{
  gchar *value;

  value = get_child_node_value (src_node, src_name);
  if (value != NULL)
    sw_contact_take (dest_contact, dest_name, value);
}

static SwContact*
_facebook_friend_node_to_contact (SwContactView *self,
                               JsonNode   *friend_node)
{
  SwFacebookContactViewPrivate *priv = GET_PRIVATE (self);
  SwContact *contact;
  char *id, *uid, *pic_url, *urls;
  char *name = NULL;
  char *updated_time = NULL;
  GStrv urls_array = NULL;
  int i;

  if (json_node_get_node_type (friend_node) != JSON_NODE_OBJECT)
    return NULL;

  contact = sw_contact_new ();
  sw_contact_set_service (contact, sw_contact_view_get_service (self));

  uid = get_child_node_value (friend_node, "id");
  if (uid == NULL)
    {
      SW_DEBUG (FACEBOOK, "Got friend without an id");
      g_object_unref (contact);

      return NULL;
    }

  if (uid != NULL)
    {
      pic_url = build_picture_url (priv->proxy, uid, FB_PICTURE_SIZE_SQUARE);
      sw_contact_request_image_fetch (contact, FALSE, "icon", pic_url);
      g_free (pic_url);
    }

  id = g_strconcat ("facebook-", uid, NULL);
  g_free (uid);
  sw_contact_take (contact, "id", id);

  name = get_child_node_value (friend_node, "name");
  if (name == NULL)
    {
      SW_DEBUG (FACEBOOK, "Got friend without a name");
      g_object_unref (contact);

      return NULL;
  }
  sw_contact_take (contact, "name", name);

  updated_time = get_child_node_value (friend_node, "updated_time");
  if (updated_time == NULL)
    {
      SW_DEBUG (FACEBOOK, "Got friend without an update time");
      g_object_unref (contact);

      return NULL;
    }
  sw_contact_take (contact, "date", updated_time);

  /* We use the vcard names as keys */
  update_contact_from_node (contact, "x-gender", friend_node, "gender");
  update_contact_from_node (contact, "url", friend_node, "link");
  urls = get_child_node_value (friend_node, "website");
  if (urls)
    urls_array = g_strsplit (urls, "\r\n", 10);
  if (urls_array) {
    for (i = 0 ; urls_array[i] != NULL ; i++) {
      sw_contact_put (contact, "url", urls_array[i]);
    }
    g_strfreev (urls_array);
    g_free (urls);
  }
  /* This is a "special" url as it's the one for the facebook profile */
  update_contact_from_node (contact, "x-facebook-profile", friend_node, "link");
  /* We could just properly set this as an "N" field and separating the
   * values with semicolons, but that would require escaping semicolons
   * and would make things more difficult for who uses libsocialweb
   * without caring about vcards */
  update_contact_from_node (contact, "n.given", friend_node, "first_name");
  update_contact_from_node (contact, "n.family", friend_node, "last_name");

  return contact;
}

static SwSet*
_facebook_update_node_to_set (SwContactView *self,
                              JsonNode   *root)
{
  SwFacebookContactViewPrivate *priv = GET_PRIVATE (self);
  JsonObject *root_object = NULL;
  JsonNode *statuses = NULL;
  JsonArray *updates_array = NULL;
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
    updates_array = json_node_get_array (statuses);
  else
    return NULL;

  set = sw_contact_set_new ();

  for (i = 0; i < json_array_get_length (updates_array); i++)
    {
      JsonNode *status;
      SwContact *contact;

      status = json_array_get_element (updates_array, i);

      contact = _facebook_friend_node_to_contact (self, status);

      if (contact != NULL)
        {
          sw_set_add (set, G_OBJECT (contact));
          g_object_unref (contact);
        }
    }

  return set;
}

static void
got_updates_cb (RestProxyCall *call,
                GError        *error,
                GObject       *weak_object,
                gpointer       userdata)
{
  SwContactView *self = SW_CONTACT_VIEW (weak_object);
  SwFacebookContactViewPrivate *priv = GET_PRIVATE (self);
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

  set = _facebook_update_node_to_set (self, root);

  json_node_free (root);

  if (set != NULL)
    {
      sw_contact_view_set_from_set (self, set);

      /* Save the results of this set to the cache */
      sw_cache_save (sw_contact_view_get_service (self),
                     priv->query,
                     priv->params,
                     set);

      sw_set_unref (set);
    }
}

static void
get_updates (SwContactView *self)
{
  SwFacebookContactViewPrivate *priv = GET_PRIVATE (self);
  RestProxyCall *call;
  const char *my_uid = sw_service_facebook_get_uid (
      (SwServiceFacebook *) sw_contact_view_get_service (self));

  if (my_uid == NULL || priv->running == 0)
    return;

  call = rest_proxy_new_call (priv->proxy);

  if (g_strcmp0 (priv->query, "people") == 0)
    {
      rest_proxy_call_set_function (call, "me/friends");
      rest_proxy_call_add_param (call, "fields",
          "updated_time,name,first_name,last_name,link,website,gender");
    }
  else
    {
      g_return_if_reached ();
    }

  rest_proxy_call_async (call,
                         (RestProxyCallAsyncCallback) got_updates_cb,
                         (GObject*) self,
                         NULL,
                         NULL);

  g_object_unref (call);
}

static void
load_from_cache (SwContactView *self)
{
  SwFacebookContactViewPrivate *priv = GET_PRIVATE (self);
  SwSet *set;

  set = sw_cache_load (sw_contact_view_get_service (self),
                       priv->query,
                       priv->params,
                       sw_contact_set_new);

  if (set != NULL)
    {
      sw_contact_view_set_from_set (self, set);

      sw_set_unref (set);
    }
}

static void
_service_user_changed (SwService  *service,
                       SwContactView *self)
{
  SwSet *set;

  /* We need to empty the set */
  set = sw_contact_set_new ();
  sw_contact_view_set_from_set (self, set);
  sw_set_unref (set);

  /* And drop the cache */
  sw_cache_drop_all (service);
}

static void
_service_capabilities_changed (SwService    *service,
                               const gchar **caps,
                               SwContactView   *self)
{
  if (sw_service_has_cap (caps, CREDENTIALS_VALID))
  {
    facebook_contact_view_refresh (self);
  }
}

static void
facebook_contact_view_constructed (GObject *self)
{
  SwService *service = sw_contact_view_get_service ((SwContactView *) self);

  g_signal_connect_object (service, "user-changed",
                    G_CALLBACK (_service_user_changed), self, 0);
  g_signal_connect_object (service, "capabilities-changed",
                    G_CALLBACK (_service_capabilities_changed), self, 0);

  if (G_OBJECT_CLASS (sw_facebook_contact_view_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (sw_facebook_contact_view_parent_class)->constructed (self);
}

static void
facebook_contact_view_set_property (GObject      *self,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  SwFacebookContactViewPrivate *priv = GET_PRIVATE (self);

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
facebook_contact_view_get_property (GObject    *self,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  SwFacebookContactViewPrivate *priv = GET_PRIVATE (self);

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
facebook_contact_view_dispose (GObject *self)
{
  SwFacebookContactViewPrivate *priv = GET_PRIVATE (self);

  facebook_contact_view_stop ((SwContactView *) self);

  g_object_unref (priv->proxy);
  priv->proxy = NULL;

  G_OBJECT_CLASS (sw_facebook_contact_view_parent_class)->dispose (self);
}

static void
facebook_contact_view_finalize (GObject *self)
{
  SwFacebookContactViewPrivate *priv = GET_PRIVATE (self);

  g_free (priv->query);
  g_boxed_free (G_TYPE_HASH_TABLE, priv->params);

  G_OBJECT_CLASS (sw_facebook_contact_view_parent_class)->finalize (self);
}

static gboolean
_update_timeout_cb (gpointer user_data)
{
   get_updates ((SwContactView *) user_data);

   return TRUE;
}

static void
facebook_contact_view_start (SwContactView *self)
{
  SwFacebookContactViewPrivate *priv = GET_PRIVATE (self);

  if (priv->running != 0)
    {
      g_message (G_STRLOC ": View asked to start, but already running");
    }
  else
    {
      SW_DEBUG (FACEBOOK, "Starting up the Facebook view");

      priv->running = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                             _update_timeout_cb,
                                             self);

      load_from_cache (self);
      get_updates (self);
    }
}

static void
facebook_contact_view_stop (SwContactView *self)
{
  SwFacebookContactViewPrivate *priv = GET_PRIVATE (self);

  if (priv->running == 0)
    {
      g_message (G_STRLOC ": View ask to stop, but not running");
    }
  else
    {
      SW_DEBUG (FACEBOOK, "Stopping the Facebook view");

      g_source_remove (priv->running);
      priv->running = 0;
    }

}

static void
facebook_contact_view_refresh (SwContactView *self)
{
  SW_DEBUG (FACEBOOK, "Forced a refresh of the Facebook view");

  get_updates (self);
}

static void
sw_facebook_contact_view_class_init (SwFacebookContactViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  SwContactViewClass *contact_view_class = SW_CONTACT_VIEW_CLASS (klass);

  gobject_class->constructed = facebook_contact_view_constructed;
  gobject_class->set_property = facebook_contact_view_set_property;
  gobject_class->get_property = facebook_contact_view_get_property;
  gobject_class->dispose = facebook_contact_view_dispose;
  gobject_class->finalize = facebook_contact_view_finalize;

  contact_view_class->start = facebook_contact_view_start;
  contact_view_class->stop = facebook_contact_view_stop;
  contact_view_class->refresh = facebook_contact_view_refresh;

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

  g_type_class_add_private (gobject_class, sizeof (SwFacebookContactViewPrivate));
}

static void
sw_facebook_contact_view_init (SwFacebookContactView *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE ((self),
      SW_TYPE_FACEBOOK_CONTACT_VIEW, SwFacebookContactViewPrivate);
}
