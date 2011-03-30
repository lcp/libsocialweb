/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2010 Intel Corporation.
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

#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-contact.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb/sw-cache.h>

#include <rest-extras/flickr-proxy.h>
#include "flickr-contact-view.h"
#include "flickr.h"

G_DEFINE_TYPE (SwFlickrContactView, sw_flickr_contact_view, SW_TYPE_CONTACT_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_FLICKR_CONTACT_VIEW, SwFlickrContactViewPrivate))

typedef struct _SwFlickrContactViewPrivate SwFlickrContactViewPrivate;

struct _SwFlickrContactViewPrivate {
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

static void _service_user_changed_cb (SwService  *service,
                                      SwContactView *contact_view);
static void _service_capabilities_changed_cb (SwService    *service,
                                              const gchar **caps,
                                              SwContactView   *contact_view);

static void
sw_flickr_contact_view_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  SwFlickrContactViewPrivate *priv = GET_PRIVATE (object);

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
sw_flickr_contact_view_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  SwFlickrContactViewPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
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
sw_flickr_contact_view_dispose (GObject *object)
{
  SwContactView *contact_view = SW_CONTACT_VIEW (object);
  SwFlickrContactViewPrivate *priv = GET_PRIVATE (object);

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

  G_OBJECT_CLASS (sw_flickr_contact_view_parent_class)->dispose (object);
}

static void
sw_flickr_contact_view_finalize (GObject *object)
{
  SwFlickrContactViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->query);
  g_hash_table_unref (priv->params);

  G_OBJECT_CLASS (sw_flickr_contact_view_parent_class)->finalize (object);
}

static SwContact *
_flickr_contact_from_node (SwServiceFlickr *service,
                           RestXmlNode     *node)
{
  const char *nsid;
  const char *realname, *username;
  const char *iconserver, *iconfarm;
  char *icon_url;
  SwContact *contact;

  nsid = rest_xml_node_get_attr (node, "nsid");
  if (!nsid)
    return NULL;

  contact = sw_contact_new ();
  sw_contact_set_service (contact, (SwService *)service);

  sw_contact_put (contact, "id", nsid);

  realname = rest_xml_node_get_attr (node, "realname");
  username = rest_xml_node_get_attr (node, "username");

  if (realname)
    sw_contact_put (contact, "fn", realname);
  if (username)
    sw_contact_put (contact, "name", username);

  sw_contact_take (contact, "date", sw_time_t_to_string (time (NULL)));

  /* The URL is built as specified by:
   * http://www.flickr.com/services/api/misc.buddyicons.html */
  iconserver = rest_xml_node_get_attr (node, "iconserver");
  iconfarm = rest_xml_node_get_attr (node, "iconfarm");
  if (iconserver && iconfarm && g_strcmp0 (iconserver, "0") != 0) {
    icon_url = g_strdup_printf (
        "http://farm%s.static.flickr.com/%s/buddyicons/%s.jpg",
        iconfarm, iconserver, nsid);
  } else {
    icon_url = g_strdup ("http://www.flickr.com/images/buddyicon.jpg");
  }
  sw_contact_request_image_fetch (contact, TRUE, "icon", icon_url);
  g_free (icon_url);

  return contact;
}

static void
_contacts_received_cb (RestProxyCall *call,
                       const GError  *error,
                       GObject       *weak_object,
                       gpointer       userdata)
{
  SwContactView *contact_view = SW_CONTACT_VIEW (weak_object);
  SwFlickrContactViewPrivate *priv = GET_PRIVATE (contact_view);
  RestXmlParser *parser;
  RestXmlNode *root, *node;
  SwSet *set;
  SwService *service;

  if (error) {
    g_warning (G_STRLOC ": Cannot get Flickr contacts: %s", error->message);
    return;
  }

  parser = rest_xml_parser_new ();
  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));
  set = sw_contact_set_new ();

  node = rest_xml_node_find (root, "contacts");

  service = sw_contact_view_get_service (contact_view);

  for (node = rest_xml_node_find (node, "contact"); node; node = node->next) {
    SwContact *contact;

    contact = _flickr_contact_from_node (SW_SERVICE_FLICKR (service),
        node);
    if (!contact)
      continue;

    if (!sw_service_is_uid_banned (service,
                                   sw_contact_get (contact, "id")))
    {
      sw_set_add (set, (GObject *)contact);
    }

    g_object_unref (contact);
  }

  rest_xml_node_unref (root);
  g_object_unref (parser);

  sw_contact_view_set_from_set (contact_view, set);
  sw_cache_save (service,
                 priv->query,
                 priv->params,
                 set);

  sw_set_unref (set);
}

static void
_make_request (SwFlickrContactView *contact_view)
{
  SwFlickrContactViewPrivate *priv = GET_PRIVATE (contact_view);
  RestProxyCall *call;
  GError *error = NULL;

  call = rest_proxy_new_call (priv->proxy);

  if (g_str_equal (priv->query, "people"))
  {
    /* http://www.flickr.com/services/api/flickr.contacts.getList.html */
    rest_proxy_call_set_function (call, "flickr.contacts.getList");
  } else {
    g_error (G_STRLOC ": Unexpected query '%s", priv->query);
  }

  if (!rest_proxy_call_async (call,
                              _contacts_received_cb,
                              (GObject *)contact_view,
                              NULL,
                              &error)) {
    g_warning (G_STRLOC ": Cannot get contacts: %s", error->message);
    g_error_free (error);
  }

  g_object_unref (call);
}

static void
_got_tokens_cb (RestProxy *proxy,
                gboolean   authorised,
                gpointer   userdata)
{
  SwFlickrContactView *contact_view = SW_FLICKR_CONTACT_VIEW (userdata);

  if (authorised) 
  {
    _make_request (contact_view);
  }

  /* Drop reference we took for callback */
  g_object_unref (contact_view);
}

static void
_get_status_updates (SwFlickrContactView *contact_view)
{
  SwFlickrContactViewPrivate *priv = GET_PRIVATE (contact_view);

  sw_keyfob_flickr ((FlickrProxy *)priv->proxy,
                    _got_tokens_cb,
                    g_object_ref (contact_view)); /* ref gets dropped in cb */
}

static gboolean
_update_timeout_cb (gpointer data)
{
  SwFlickrContactView *contact_view = SW_FLICKR_CONTACT_VIEW (data);

  _get_status_updates (contact_view);

  return TRUE;
}

static void
_load_from_cache (SwFlickrContactView *contact_view)
{
  SwFlickrContactViewPrivate *priv = GET_PRIVATE (contact_view);
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
flickr_contact_view_start (SwContactView *contact_view)
{
  SwFlickrContactViewPrivate *priv = GET_PRIVATE (contact_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              contact_view);
    _load_from_cache ((SwFlickrContactView *)contact_view);
    _get_status_updates ((SwFlickrContactView *)contact_view);
  }
}

static void
flickr_contact_view_stop (SwContactView *contact_view)
{
  SwFlickrContactViewPrivate *priv = GET_PRIVATE (contact_view);

  if (!priv->timeout_id)
  {
    g_warning (G_STRLOC ": View not running");
  } else {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }
}

static void
flickr_contact_view_refresh (SwContactView *contact_view)
{
  _get_status_updates ((SwFlickrContactView *)contact_view);
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
  SwFlickrContactViewPrivate *priv = GET_PRIVATE ((SwFlickrContactView*) contact_view);

  if (sw_service_has_cap (caps, CREDENTIALS_VALID))
  {
    flickr_contact_view_refresh (contact_view);
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
sw_flickr_contact_view_constructed (GObject *object)
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

  if (G_OBJECT_CLASS (sw_flickr_contact_view_parent_class)->constructed)
    G_OBJECT_CLASS (sw_flickr_contact_view_parent_class)->constructed (object);
}

static void
sw_flickr_contact_view_class_init (SwFlickrContactViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwContactViewClass *contact_view_class = SW_CONTACT_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwFlickrContactViewPrivate));

  object_class->get_property = sw_flickr_contact_view_get_property;
  object_class->set_property = sw_flickr_contact_view_set_property;
  object_class->dispose = sw_flickr_contact_view_dispose;
  object_class->finalize = sw_flickr_contact_view_finalize;
  object_class->constructed = sw_flickr_contact_view_constructed;

  contact_view_class->start = flickr_contact_view_start;
  contact_view_class->stop = flickr_contact_view_stop;
  contact_view_class->refresh = flickr_contact_view_refresh;

  pspec = g_param_spec_object ("proxy",
                               "proxy",
                               "proxy",
                               REST_TYPE_PROXY,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PROXY, pspec);

  pspec = g_param_spec_boxed ("params",
                              "params",
                              "params",
                              G_TYPE_HASH_TABLE,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PARAMS, pspec);

  pspec = g_param_spec_string ("query",
                               "query",
                               "query",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_QUERY, pspec);
}

static void
sw_flickr_contact_view_init (SwFlickrContactView *self)
{
}
