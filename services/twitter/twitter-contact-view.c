/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 * Copyright (C) 2011 Collabora Ltd.
 *
 * Author: Rob Bradford <rob@linux.intel.com>
 *         Alban Crequy <alban.crequy@collabora.co.uk>
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
#include <libsocialweb/sw-utils.h>

#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-contact.h>
#include <libsocialweb/sw-cache.h>
#include <libsocialweb/sw-call-list.h>
#include <libsocialweb/sw-utils.h>

#include "twitter.h"
#include "twitter-contact-view.h"

/* Lookup up to TWITTER_LOOKUP_MAX users in one request
 * According to the documentation, it is supposed to be 100 but it does not
 * work at the moment
 * http://dev.twitter.com/doc/get/users/lookup
 */
#define TWITTER_LOOKUP_MAX 1

G_DEFINE_TYPE (SwTwitterContactView,
               sw_twitter_contact_view,
               SW_TYPE_CONTACT_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_TWITTER_CONTACT_VIEW, SwTwitterContactViewPrivate))

typedef struct _SwTwitterContactViewPrivate SwTwitterContactViewPrivate;

struct _SwTwitterContactViewPrivate {
  RestProxy *proxy;
  guint timeout_id;
  GHashTable *params;
  gchar *query;

  SwCallList *calls;
  SwSet *set;
};

enum
{
  PROP_0,
  PROP_PROXY,
  PROP_PARAMS,
  PROP_QUERY
};

#define UPDATE_TIMEOUT 5 * 60

static void _service_user_changed_cb (SwService  *service,
                                      SwContactView *contact_view);
static void _service_capabilities_changed_cb (SwService    *service,
                                              const gchar **caps,
                                              SwContactView   *contact_view);

static void
sw_twitter_contact_view_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  SwTwitterContactViewPrivate *priv = GET_PRIVATE (object);

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
sw_twitter_contact_view_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  SwTwitterContactViewPrivate *priv = GET_PRIVATE (object);

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
sw_twitter_contact_view_dispose (GObject *object)
{
  SwContactView *contact_view = SW_CONTACT_VIEW (object);
  SwTwitterContactViewPrivate *priv = GET_PRIVATE (object);

  if (priv->calls)
  {
    sw_call_list_free (priv->calls);
    priv->calls = NULL;
  }

  if (priv->proxy)
  {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->timeout_id)
  {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }

  g_signal_handlers_disconnect_by_func (sw_contact_view_get_service (contact_view),
                                        _service_user_changed_cb,
                                        contact_view);
  g_signal_handlers_disconnect_by_func (sw_contact_view_get_service (contact_view),
                                        _service_capabilities_changed_cb,
                                        contact_view);

  G_OBJECT_CLASS (sw_twitter_contact_view_parent_class)->dispose (object);
}

static void
sw_twitter_contact_view_finalize (GObject *object)
{
  SwTwitterContactViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->query);
  g_hash_table_unref (priv->params);

  G_OBJECT_CLASS (sw_twitter_contact_view_parent_class)->finalize (object);
}

static SwContact *
_make_contact (SwTwitterContactView *contact_view,
               RestXmlNode           *node)
{
  RestXmlNode *n;
  const char *user_id, *user_name;
  const char *url;
  SwContact *contact;

  if (!node)
    return NULL;

  user_id = rest_xml_node_find (node, "screen_name")->content;
  if (!user_id)
    return NULL;


  contact = sw_contact_new ();

  sw_contact_put (contact, "id", user_id);
  
  url = rest_xml_node_find (node, "url")->content;
  if (url)
    sw_contact_put (contact, "url", url);

  user_name = rest_xml_node_find (node, "name")->content;
  if (user_name)
    sw_contact_put (contact, "name", user_name);

  sw_contact_take (contact, "date", sw_time_t_to_string (time (NULL)));

  n = rest_xml_node_find (node, "profile_image_url");
  if (n && n->content)
    sw_contact_request_image_fetch (contact, FALSE, "icon", n->content);

  return contact;
}

static RestXmlNode *
_make_node_from_call (RestProxyCall *call)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *root;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_warning (G_STRLOC ": Error from Twitter: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  if (root == NULL) {
    g_warning (G_STRLOC ": Error parsing payload from Twitter: %s",
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  return root;
}


static void
_update_if_done (SwTwitterContactView *contact_view)
{
  SwTwitterContactViewPrivate *priv = GET_PRIVATE (contact_view);
  
  if (sw_call_list_is_empty (priv->calls))
  {
    SwService *service = sw_contact_view_get_service
        (SW_CONTACT_VIEW (contact_view));

    sw_contact_view_set_from_set ((SwContactView *)contact_view, priv->set);

    /* Save the results of this set to the cache */
    sw_cache_save (service,
                   priv->query,
                   priv->params,
                   priv->set);

    sw_set_empty (priv->set);
  }
}


static void
_got_contacts_updates_cb (RestProxyCall *call,
                          const GError  *error,
                          GObject       *weak_object,
                          gpointer       userdata)
{
  SwTwitterContactView *contact_view = SW_TWITTER_CONTACT_VIEW (weak_object);
  SwTwitterContactViewPrivate *priv = GET_PRIVATE (contact_view);
  RestXmlNode *root, *node;
  SwService *service;

  sw_call_list_remove (priv->calls, call);

  if (error) {
    g_warning (G_STRLOC ": Error getting contacts: %s", error->message);
    return;
  }

  root = _make_node_from_call (call);

  if (!root)
    return;

  SW_DEBUG (TWITTER, "Got contacts!");

  service = sw_contact_view_get_service (SW_CONTACT_VIEW (contact_view));

  for (node = root; node; node = node->next)
  {
    SwContact *contact;

    contact = _make_contact (contact_view, node);
    sw_contact_set_service (contact, service);

    if (contact)
    {
      if (!sw_service_is_uid_banned (service,
                                     sw_contact_get (contact, "id")))
      {
        sw_set_add (priv->set, (GObject *)contact);
      }

      g_object_unref (contact);
    }
  }

  rest_xml_node_unref (root);

  _update_if_done (contact_view);
}


static void
_got_ids_cb (RestProxyCall *call,
             const GError  *error,
             GObject       *weak_object,
             gpointer       userdata)
{
  SwTwitterContactView *contact_view = SW_TWITTER_CONTACT_VIEW (weak_object);
  SwTwitterContactViewPrivate *priv = GET_PRIVATE (contact_view);
  RestXmlNode *root, *node;
  SwService *service;
  char *ids;
  int i;

  sw_call_list_remove (priv->calls, call);

  if (error) {
    g_warning (G_STRLOC ": Error getting contact ids: %s", error->message);
    return;
  }

  root = _make_node_from_call (call);

  if (!root)
    return;

  SW_DEBUG (TWITTER, "Got ids!");

  service = sw_contact_view_get_service (SW_CONTACT_VIEW (contact_view));

  i = 0;
  ids = NULL;
  for (node = rest_xml_node_find (root, "id"); node; node = node->next)
  {
    if (!ids)
    {
      ids = g_strdup (node->content);
    }
    else
    {
      char *new_ids;
      new_ids = g_strdup_printf ("%s,%s", ids, node->content);
      g_free (ids);
      ids = new_ids;
    }

    i++;
    if (i == TWITTER_LOOKUP_MAX || !node->next)
    {
      call = rest_proxy_new_call (priv->proxy);
      rest_proxy_call_set_function (call, "users/lookup.xml");
      sw_call_list_add (priv->calls, call);

      rest_proxy_call_add_params (call,
                                  "user_id", ids,
                                  NULL);

      rest_proxy_call_async (call,
                             _got_contacts_updates_cb,
                             (GObject *)contact_view,
                             NULL,
                             NULL);
      i = 0;
      g_free (ids);
      ids = NULL;
    }
  }

  rest_xml_node_unref (root);
}

static void
_get_ids (SwTwitterContactView *contact_view)
{
  SwTwitterContactViewPrivate *priv = GET_PRIVATE (contact_view);
  RestProxyCall *call;
  SwService *service;
  const char *username;

  call = rest_proxy_new_call (priv->proxy);

  if (g_str_equal (priv->query, "people"))
  {
    /* http://dev.twitter.com/doc/get/users/lookup */
    rest_proxy_call_set_function (call, "friends/ids.xml");
  } else {
    g_error (G_STRLOC ": Unexpected query '%s", priv->query);
  }

  sw_call_list_cancel_all (priv->calls);
  sw_set_empty (priv->set);

  service = sw_contact_view_get_service (SW_CONTACT_VIEW (contact_view));
  username = sw_service_twitter_get_username (SW_SERVICE_TWITTER (service));
  rest_proxy_call_add_params (call,
                              "screen_name", username,
                              NULL);

  rest_proxy_call_async (call,
                         _got_ids_cb,
                         (GObject*)contact_view,
                         NULL,
                         NULL);

  g_object_unref (call);
}

static gboolean
_update_timeout_cb (gpointer data)
{
  SwTwitterContactView *contact_view = SW_TWITTER_CONTACT_VIEW (data);

  _get_ids (contact_view);

  return TRUE;
}

static void
_load_from_cache (SwTwitterContactView *contact_view)
{
  SwTwitterContactViewPrivate *priv = GET_PRIVATE (contact_view);
  SwSet *set;

  set = sw_cache_load (sw_contact_view_get_service (SW_CONTACT_VIEW (contact_view)),
                       priv->query,
                       priv->params,
                       sw_contact_set_new);

  if (set)
  {
    sw_contact_view_set_from_set (SW_CONTACT_VIEW (contact_view),
                               set);
    sw_set_unref (set);
  }
}

static void
twitter_contact_view_start (SwContactView *contact_view)
{
  SwTwitterContactViewPrivate *priv = GET_PRIVATE (contact_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    SW_DEBUG (TWITTER, G_STRLOC ": Setting up the timeout");
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              contact_view);

    _load_from_cache ((SwTwitterContactView *)contact_view);
    _get_ids ((SwTwitterContactView *)contact_view);
  }
}

static void
twitter_contact_view_stop (SwContactView *contact_view)
{
  SwTwitterContactViewPrivate *priv = GET_PRIVATE (contact_view);

  if (!priv->timeout_id)
  {
    g_warning (G_STRLOC ": View not running");
  } else {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }
}

static void
twitter_contact_view_refresh (SwContactView *contact_view)
{
  _get_ids ((SwTwitterContactView *)contact_view);
}

static void
_service_user_changed_cb (SwService  *service,
                          SwContactView *contact_view)
{
  SwSet *set;

  /* We need to empty the set */
  set = sw_contact_set_new ();
  sw_contact_view_set_from_set (SW_CONTACT_VIEW (contact_view),
                             set);
  sw_set_unref (set);

  /* And drop the cache */
  sw_cache_drop_all (service);
}

static void
_service_capabilities_changed_cb (SwService    *service,
                                  const gchar **caps,
                                  SwContactView   *contact_view)
{
  SwTwitterContactViewPrivate *priv = GET_PRIVATE ((SwTwitterContactView*) contact_view);

  if (sw_service_has_cap (caps, CREDENTIALS_VALID))
  {
    twitter_contact_view_refresh (contact_view);
    if (!priv->timeout_id)
    {
      priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                                (GSourceFunc)_update_timeout_cb,
                                                contact_view);
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
sw_twitter_contact_view_constructed (GObject *object)
{
  SwContactView *contact_view = SW_CONTACT_VIEW (object);

  g_signal_connect (sw_contact_view_get_service (contact_view),
                    "user-changed",
                    (GCallback)_service_user_changed_cb,
                    contact_view);

  g_signal_connect (sw_contact_view_get_service (contact_view),
                    "capabilities-changed",
                    (GCallback)_service_capabilities_changed_cb,
                    contact_view);

  if (G_OBJECT_CLASS (sw_twitter_contact_view_parent_class)->constructed)
    G_OBJECT_CLASS (sw_twitter_contact_view_parent_class)->constructed (object);
}

static void
sw_twitter_contact_view_class_init (SwTwitterContactViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwContactViewClass *contact_view_class = SW_CONTACT_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwTwitterContactViewPrivate));

  object_class->get_property = sw_twitter_contact_view_get_property;
  object_class->set_property = sw_twitter_contact_view_set_property;
  object_class->dispose = sw_twitter_contact_view_dispose;
  object_class->finalize = sw_twitter_contact_view_finalize;
  object_class->constructed = sw_twitter_contact_view_constructed;

  contact_view_class->start = twitter_contact_view_start;
  contact_view_class->stop = twitter_contact_view_stop;
  contact_view_class->refresh = twitter_contact_view_refresh;

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
sw_twitter_contact_view_init (SwTwitterContactView *self)
{
  SwTwitterContactViewPrivate *priv = GET_PRIVATE (self);

  priv->calls = sw_call_list_new ();
  priv->set = sw_contact_set_new ();
}
