/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2010 Intel Corporation.
 * Copyright (C) 2011 Collabora Ltd.
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
#include <libsocialweb/sw-cache.h>
#include <libsocialweb/sw-call-list.h>

#include <libsocialweb-keystore/sw-keystore.h>

#include <glib/gi18n.h>


#include "lastfm-contact-view.h"
#include "lastfm.h"


G_DEFINE_TYPE (SwLastfmContactView, sw_lastfm_contact_view, SW_TYPE_CONTACT_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_LASTFM_CONTACT_VIEW, SwLastfmContactViewPrivate))

typedef struct _SwLastfmContactViewPrivate SwLastfmContactViewPrivate;

struct _SwLastfmContactViewPrivate {
  guint timeout_id;
  GHashTable *params;
  gchar *query;
  RestProxy *proxy;

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
sw_lastfm_contact_view_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  SwLastfmContactViewPrivate *priv = GET_PRIVATE (object);

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
sw_lastfm_contact_view_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  SwLastfmContactViewPrivate *priv = GET_PRIVATE (object);

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
sw_lastfm_contact_view_dispose (GObject *object)
{
  SwContactView *contact_view = SW_CONTACT_VIEW (object);
  SwLastfmContactViewPrivate *priv = GET_PRIVATE (object);

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

  if (priv->calls)
  {
    sw_call_list_free (priv->calls);
    priv->calls = NULL;
  }

  g_signal_handlers_disconnect_by_func (sw_contact_view_get_service (contact_view),
                                        _service_user_changed_cb,
                                        contact_view);
  g_signal_handlers_disconnect_by_func (sw_contact_view_get_service (contact_view),
                                        _service_capabilities_changed_cb,
                                        contact_view);

  G_OBJECT_CLASS (sw_lastfm_contact_view_parent_class)->dispose (object);
}

static void
sw_lastfm_contact_view_finalize (GObject *object)
{
  SwLastfmContactViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->query);
  g_hash_table_unref (priv->params);

  G_OBJECT_CLASS (sw_lastfm_contact_view_parent_class)->finalize (object);
}

static RestXmlNode *
node_from_call (RestProxyCall *call)
{
  static RestXmlParser *parser = NULL;
  RestXmlNode *node;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_message (G_STRLOC ": error from Last.fm: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  node = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  /* No content, or wrong content */
  if (node == NULL || strcmp (node->name, "lfm") != 0) {
    g_message (G_STRLOC ": cannot make Last.fm call");
    /* TODO: display the payload if its short */
    if (node) rest_xml_node_unref (node);
    return NULL;
  }

  if (strcmp (rest_xml_node_get_attr (node, "status"), "ok") != 0) {
    RestXmlNode *err_node;
    err_node = rest_xml_node_find (node, "error");
    g_message (G_STRLOC ": cannot make Last.fm call: %s (code %s)",
                err_node->content,
                rest_xml_node_get_attr (err_node, "code"));
    rest_xml_node_unref (node);
    return NULL;
  }

  return node;
}

static void
_update_if_done (SwLastfmContactView *contact_view)
{
  SwLastfmContactViewPrivate *priv = GET_PRIVATE (contact_view);
  
  if (sw_call_list_is_empty (priv->calls))
  {
    SwService *service = sw_contact_view_get_service (SW_CONTACT_VIEW (contact_view));

    SW_DEBUG (LASTFM, "Call set is empty, emitting refreshed signal");
    sw_contact_view_set_from_set ((SwContactView *)contact_view, priv->set);

    /* Save the results of this set to the cache */
    sw_cache_save (service,
                   priv->query,
                   priv->params,
                   priv->set);

    sw_set_empty (priv->set);
  } else {
    SW_DEBUG (LASTFM, "Call set is not empty, still more work to do.");
  }
}

static const char *
get_image_url (RestXmlNode *node,
               const char  *size)
{
  g_assert (node);
  g_assert (size);

  for (node = rest_xml_node_find (node, "image"); node; node = node->next) {
    /* Skip over images which are not medium sized */
    if (!g_str_equal (rest_xml_node_get_attr (node, "size"), size))
      continue;

    if (node->content) {
      return node->content;
    } else {
      /* TODO: should fetch another size instead */
      return NULL;
    }
  }

  return NULL;
}

static void
get_artist_info_cb (RestProxyCall *call,
                    const GError  *error,
                    GObject       *weak_object,
                    gpointer       user_data)
{
  SwLastfmContactView *contact_view = SW_LASTFM_CONTACT_VIEW (weak_object);
  SwLastfmContactViewPrivate *priv = GET_PRIVATE (contact_view);
  SwContact *contact = user_data;
  RestXmlNode *root, *artist_node;
  const char *url;

  sw_call_list_remove (priv->calls, call);

  if (error) {
    g_message (G_STRLOC ": error from Last.fm: %s", error->message);
    g_object_unref (call);
    g_object_unref (contact);
    return;
  }

  root = node_from_call (call);
  g_object_unref (call);
  if (!root)
    return;

  artist_node = rest_xml_node_find (root, "artist");
  url = get_image_url (artist_node, "large");
  if (url)
    sw_contact_request_image_fetch (contact, TRUE, "thumbnail", url);

  sw_contact_pop_pending (contact);
  g_object_unref (contact);

  _update_if_done (contact_view);

  rest_xml_node_unref (root);
}

static void
get_thumbnail (SwLastfmContactView *contact_view,
               SwContact           *contact,
               RestXmlNode      *track_node)
{
  SwLastfmContactViewPrivate *priv = GET_PRIVATE (contact_view);
  const char *url;
  RestProxyCall *call;
  RestXmlNode *artist;
  const char *mbid;

  url = get_image_url (track_node, "large");
  if (url) {
    sw_contact_request_image_fetch (contact, TRUE, "thumbnail", url);
    return;
  }

  /* If we didn't find an album image, then try the artist image */

  sw_contact_push_pending (contact);

  call = rest_proxy_new_call (priv->proxy);
  sw_call_list_add (priv->calls, call);

  rest_proxy_call_add_params (call,
                              "method", "artist.getInfo",
                              "api_key", sw_keystore_get_key ("lastfm"),
                              NULL);

  artist = rest_xml_node_find (track_node, "artist");
  mbid = rest_xml_node_get_attr (artist, "mbid");
  if (mbid && mbid[0] != '\0') {
    rest_proxy_call_add_param (call, "mbid", mbid);
  } else {
    rest_proxy_call_add_param (call, "artist", artist->content);
  }

  rest_proxy_call_async (call,
                         get_artist_info_cb,
                         (GObject *)contact_view,
                         g_object_ref (contact),
                         NULL);
}

static char *
make_title (RestXmlNode *node)
{
  const char *track, *artist;

  track = rest_xml_node_find (node, "name")->content;
  artist = rest_xml_node_find (node, "artist")->content;

  if (track && artist) {
    /* Translators "[track title] by [artist]" */
    return g_strdup_printf (_("%s by %s"), track, artist);
  } else if (track) {
    return g_strdup (track);
  } else {
    return g_strdup (_("Unknown"));
  }
}

static SwContact *
make_contact (SwLastfmContactView *contact_view,
           SwService        *service,
           RestXmlNode      *user,
           RestXmlNode      *track)
{
  RestXmlNode *date;
  SwContact *contact;
  const char *s;
  char *id;

  contact = sw_contact_new ();
  sw_contact_set_service (contact, service);

  id = g_strdup_printf ("%s %s",
                        rest_xml_node_find (track, "url")->content,
                        rest_xml_node_find (user, "name")->content);
  sw_contact_take (contact, "id", id);
  sw_contact_put (contact, "url", rest_xml_node_find (track, "url")->content);
  sw_contact_take (contact, "title", make_title (track));
  sw_contact_put (contact, "album", rest_xml_node_find (track, "album")->content);

  get_thumbnail (contact_view, contact, track);

  date = rest_xml_node_find (track, "date");
  if (date) {
    sw_contact_take (contact, "date", sw_time_t_to_string (atoi (rest_xml_node_get_attr (date, "uts"))));
  } else {
    /* No date means it's a now-playing contact, so use now at the timestamp */
    sw_contact_take (contact, "date", sw_time_t_to_string (time (NULL)));
  }

  s = rest_xml_node_find (user, "realname")->content;
  if (s) {
    sw_contact_put (contact, "author", s);
  } else {
    sw_contact_put (contact, "author", rest_xml_node_find (user, "name")->content);
  }

  sw_contact_put (contact, "authorid", rest_xml_node_find (user, "name")->content);

  s = get_image_url (user, "medium");
  if (s)
    sw_contact_request_image_fetch (contact, FALSE, "authoricon", s);

  return contact;
}

static void
_get_friends_cb (RestProxyCall *call,
                 const GError  *error,
                 GObject       *weak_object,
                 gpointer       user_data)
{
  SwLastfmContactView *contact_view = SW_LASTFM_CONTACT_VIEW (weak_object);
  SwLastfmContactViewPrivate *priv = GET_PRIVATE (weak_object);
  gboolean updated = FALSE;

  RestXmlNode *root, *node;

  sw_call_list_remove (priv->calls, call);

  if (error) {
    g_message (G_STRLOC ": error from Last.fm: %s", error->message);
    g_object_unref (call);
    return;
  }

  SW_DEBUG (LASTFM, "Got result of getFriends call");

  root = node_from_call (call);
  g_object_unref (call);
  if (!root)
    return;

  SW_DEBUG (LASTFM, "Parsed results of getFriends call");

  for (node = rest_xml_node_find (root, "user"); node; node = node->next)
  {
    SwContact *contact;
    SwService *service;
    const gchar *id;
    const gchar *realname;
    const gchar *url;

    service = sw_contact_view_get_service (SW_CONTACT_VIEW (contact_view));
    contact = sw_contact_new ();
    sw_contact_set_service (contact, service);

    id = rest_xml_node_find (node, "name")->content;
    realname = rest_xml_node_find (node, "realname")->content;
    url = rest_xml_node_find (node, "url")->content;

    if (!id) {
      g_object_unref (contact);
      continue;
    }
    sw_contact_put (contact, "id", id);
    if (!realname)
      realname = id;
    sw_contact_put (contact, "name", realname);
    if (url)
      sw_contact_put (contact, "url", url);

    if (!sw_service_is_uid_banned (service,
                                   sw_contact_get (contact, "id")))
    {
      sw_set_add (priv->set, (GObject *)contact);
      updated = TRUE;
    }

    /* No date, so use now at the timestamp */
    sw_contact_take (contact, "date", sw_time_t_to_string (time (NULL)));

    g_object_unref (contact);
  }

  rest_xml_node_unref (root);

  if (updated)
    _update_if_done (contact_view);
}

static void
_get_updates (SwLastfmContactView *contact_view)
{
  SwLastfmContactViewPrivate *priv = GET_PRIVATE (contact_view);
  SwService *service;
  RestProxyCall *call;
  const gchar *user_id;

  sw_call_list_cancel_all (priv->calls);
  sw_set_empty (priv->set);

  SW_DEBUG (LASTFM, "Making getFriends call");
  call = rest_proxy_new_call (priv->proxy);
  sw_call_list_add (priv->calls, call);

  service = sw_contact_view_get_service (SW_CONTACT_VIEW (contact_view));
  user_id = sw_service_lastfm_get_user_id (SW_SERVICE_LASTFM (service));

  if (!user_id)
  {
    /* Not yet configured */
    return;
  }

  rest_proxy_call_add_params (call,
                              "api_key", sw_keystore_get_key ("lastfm"),
                              "user", user_id,
                              "method", "user.getFriends",
                              NULL);
  rest_proxy_call_async (call,
                         _get_friends_cb,
                         (GObject *)contact_view,
                         NULL,
                         NULL);
}


static gboolean
_update_timeout_cb (gpointer data)
{
  SwLastfmContactView *contact_view = SW_LASTFM_CONTACT_VIEW (data);

  _get_updates (contact_view);

  return TRUE;
}

static void
_load_from_cache (SwLastfmContactView *contact_view)
{
  SwLastfmContactViewPrivate *priv = GET_PRIVATE (contact_view);
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
lastfm_contact_view_start (SwContactView *contact_view)
{
  SwLastfmContactViewPrivate *priv = GET_PRIVATE (contact_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    SW_DEBUG (LASTFM, G_STRLOC ": Setting up the timeout");
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              contact_view);

    _load_from_cache ((SwLastfmContactView *)contact_view);
    _get_updates ((SwLastfmContactView *)contact_view);
  }
}

static void
lastfm_contact_view_stop (SwContactView *contact_view)
{
  SwLastfmContactViewPrivate *priv = GET_PRIVATE (contact_view);

  if (!priv->timeout_id)
  {
    g_warning (G_STRLOC ": View not running");
  } else {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }
}

static void
lastfm_contact_view_refresh (SwContactView *contact_view)
{
  _get_updates ((SwLastfmContactView *)contact_view);
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
  SwLastfmContactViewPrivate *priv = GET_PRIVATE ((SwLastfmContactView*) contact_view);

  if (sw_service_has_cap (caps, CREDENTIALS_VALID))
  {
    lastfm_contact_view_refresh (contact_view);
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
sw_lastfm_contact_view_constructed (GObject *object)
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


  if (G_OBJECT_CLASS (sw_lastfm_contact_view_parent_class)->constructed)
    G_OBJECT_CLASS (sw_lastfm_contact_view_parent_class)->constructed (object);
}

static void
sw_lastfm_contact_view_class_init (SwLastfmContactViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwContactViewClass *contact_view_class = SW_CONTACT_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwLastfmContactViewPrivate));

  object_class->get_property = sw_lastfm_contact_view_get_property;
  object_class->set_property = sw_lastfm_contact_view_set_property;
  object_class->dispose = sw_lastfm_contact_view_dispose;
  object_class->finalize = sw_lastfm_contact_view_finalize;
  object_class->constructed = sw_lastfm_contact_view_constructed;

  contact_view_class->start = lastfm_contact_view_start;
  contact_view_class->stop = lastfm_contact_view_stop;
  contact_view_class->refresh = lastfm_contact_view_refresh;

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
sw_lastfm_contact_view_init (SwLastfmContactView *self)
{
  SwLastfmContactViewPrivate *priv = GET_PRIVATE (self);

  priv->calls = sw_call_list_new ();
  priv->set = sw_contact_set_new ();
}


