/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2010 Intel Corporation.
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
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-cache.h>
#include <libsocialweb/sw-call-list.h>

#include <libsocialweb-keystore/sw-keystore.h>

#include <glib/gi18n.h>


#include "lastfm-item-view.h"
#include "lastfm.h"


G_DEFINE_TYPE (SwLastfmItemView, sw_lastfm_item_view, SW_TYPE_ITEM_VIEW)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_LASTFM_ITEM_VIEW, SwLastfmItemViewPrivate))

typedef struct _SwLastfmItemViewPrivate SwLastfmItemViewPrivate;

struct _SwLastfmItemViewPrivate {
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

static void
sw_lastfm_item_view_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  SwLastfmItemViewPrivate *priv = GET_PRIVATE (object);

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
sw_lastfm_item_view_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  SwLastfmItemViewPrivate *priv = GET_PRIVATE (object);

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
sw_lastfm_item_view_dispose (GObject *object)
{
  SwLastfmItemViewPrivate *priv = GET_PRIVATE (object);

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

  G_OBJECT_CLASS (sw_lastfm_item_view_parent_class)->dispose (object);
}

static void
sw_lastfm_item_view_finalize (GObject *object)
{
  SwLastfmItemViewPrivate *priv = GET_PRIVATE (object);

  g_free (priv->query);
  g_hash_table_unref (priv->params);

  G_OBJECT_CLASS (sw_lastfm_item_view_parent_class)->finalize (object);
}

static RestXmlNode *
node_from_call (RestProxyCall *call)
{
  static RestXmlParser *parser = NULL;
  GError *error = NULL;
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
  g_object_unref (call);

  /* No content, or wrong content */
  if (node == NULL || strcmp (node->name, "lfm") != 0) {
    g_message (G_STRLOC ": cannot make Last.fm call: %s",
                error ? error->message : "unknown reason");
    if (error) g_error_free (error);
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
_update_if_done (SwLastfmItemView *item_view)
{
  SwLastfmItemViewPrivate *priv = GET_PRIVATE (item_view);
  
  if (sw_call_list_is_empty (priv->calls))
  {
    SwService *service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));

    SW_DEBUG (LASTFM, "Call set is empty, emitting refreshed signal");
    sw_item_view_set_from_set ((SwItemView *)item_view, priv->set);

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
  SwLastfmItemView *item_view = SW_LASTFM_ITEM_VIEW (weak_object);
  SwLastfmItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwItem *item = user_data;
  RestXmlNode *root, *artist_node;
  const char *url;

  sw_call_list_remove (priv->calls, call);

  if (error) {
    g_message (G_STRLOC ": error from Last.fm: %s", error->message);
    /* TODO: cleanup or something */
    return;
  }

  root = node_from_call (call);
  if (!root)
    return;

  artist_node = rest_xml_node_find (root, "artist");
  url = get_image_url (artist_node, "large");
  if (url)
    sw_item_request_image_fetch (item, TRUE, "thumbnail", url);

  sw_item_pop_pending (item);

  _update_if_done (item_view);
}

static void
get_thumbnail (SwLastfmItemView *item_view,
               SwItem           *item,
               RestXmlNode      *track_node)
{
  SwLastfmItemViewPrivate *priv = GET_PRIVATE (item_view);
  const char *url;
  RestProxyCall *call;
  RestXmlNode *artist;
  const char *mbid;

  url = get_image_url (track_node, "large");
  if (url) {
    sw_item_request_image_fetch (item, TRUE, "thumbnail", url);
    return;
  }

  /* If we didn't find an album image, then try the artist image */

  sw_item_push_pending (item);

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
                         (GObject *)item_view,
                         item,
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

static SwItem *
make_item (SwLastfmItemView *item_view,
           SwService        *service,
           RestXmlNode      *user,
           RestXmlNode      *track)
{
  RestXmlNode *date;
  SwItem *item;
  const char *s;
  char *id;

  item = sw_item_new ();
  sw_item_set_service (item, service);

  id = g_strdup_printf ("%s %s",
                        rest_xml_node_find (track, "url")->content,
                        rest_xml_node_find (user, "name")->content);
  sw_item_take (item, "id", id);
  sw_item_put (item, "url", rest_xml_node_find (track, "url")->content);
  sw_item_take (item, "title", make_title (track));
  sw_item_put (item, "album", rest_xml_node_find (track, "album")->content);

  get_thumbnail (item_view, item, track);

  date = rest_xml_node_find (track, "date");
  if (date) {
    sw_item_take (item, "date", sw_time_t_to_string (atoi (rest_xml_node_get_attr (date, "uts"))));
  } else {
    /* No date means it's a now-playing item, so use now at the timestamp */
    sw_item_take (item, "date", sw_time_t_to_string (time (NULL)));
  }

  s = rest_xml_node_find (user, "realname")->content;
  if (s) {
    sw_item_put (item, "author", s);
  } else {
    sw_item_put (item, "author", rest_xml_node_find (user, "name")->content);
  }

  sw_item_put (item, "authorid", rest_xml_node_find (user, "name")->content);

  s = get_image_url (user, "medium");
  if (s)
    sw_item_request_image_fetch (item, FALSE, "authoricon", s);

  return item;
}

static void
_get_tracks_cb (RestProxyCall *call,
                const GError  *error,
                GObject       *weak_object,
                gpointer       user_data)
{
  SwLastfmItemView *item_view = SW_LASTFM_ITEM_VIEW (weak_object);
  SwLastfmItemViewPrivate *priv = GET_PRIVATE (item_view);
  RestXmlNode *user_node = user_data;
  RestXmlNode *root, *track_node;
  SwService *service;


  sw_call_list_remove (priv->calls, call);

  if (error) {
    g_message (G_STRLOC ": error from Last.fm: %s", error->message);
    /* TODO: cleanup or something */
    return;
  }

  SW_DEBUG (LASTFM, "Got results for getTracks call");

  root = node_from_call (call);
  if (!root)
    return;

  SW_DEBUG (LASTFM, "Parsed results for getTracks call");


  service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));

  /* Although we only asked for a single track, if there is now-playing data
     then that is returned as well. */
  for (track_node = rest_xml_node_find (root, "track"); track_node; track_node = track_node->next) 
  {
    SwItem *item;

    item = make_item (item_view, service, user_node, track_node);
    sw_set_add (priv->set, (GObject *)item);
  }

  rest_xml_node_unref (root);

  _update_if_done (item_view);
}

static void
_get_friends_cb (RestProxyCall *call,
                 const GError  *error,
                 GObject       *weak_object,
                 gpointer       user_data)
{
  SwLastfmItemView *item_view = SW_LASTFM_ITEM_VIEW (weak_object);
  SwLastfmItemViewPrivate *priv = GET_PRIVATE (weak_object);

  RestXmlNode *root, *node;

  sw_call_list_remove (priv->calls, call);

  if (error) {
    g_message (G_STRLOC ": error from Last.fm: %s", error->message);
    return;
  }

  SW_DEBUG (LASTFM, "Got result of getFriends call");

  root = node_from_call (call);
  if (!root)
    return;

  SW_DEBUG (LASTFM, "Parsed results of getFriends call");

  for (node = rest_xml_node_find (root, "user"); node; node = node->next)
  {
    call = rest_proxy_new_call (priv->proxy);
    sw_call_list_add (priv->calls, call);

    SW_DEBUG (LASTFM, "Making getRecentTracks call for %s",
              rest_xml_node_find (node, "name")->content);

    rest_proxy_call_add_params (call,
                                "api_key", sw_keystore_get_key ("lastfm"),
                                "method", "user.getRecentTracks",
                                "user", rest_xml_node_find (node, "name")->content,
                                "limit", "1",
                                NULL);

    rest_proxy_call_async (call,
                           _get_tracks_cb,
                           (GObject *)item_view,
                           rest_xml_node_ref (node),
                           NULL);
  }

  rest_xml_node_unref (root);
}

static void
_get_status_updates (SwLastfmItemView *item_view)
{
  SwLastfmItemViewPrivate *priv = GET_PRIVATE (item_view);
  SwService *service;
  RestProxyCall *call;
  const gchar *user_id;

  if (!g_str_equal (priv->query, "feed"))
  {
    g_error (G_STRLOC ": Unexpected query '%s'", priv->query);
  }

  sw_call_list_cancel_all (priv->calls);
  sw_set_empty (priv->set);

  SW_DEBUG (LASTFM, "Making getFriends call");
  call = rest_proxy_new_call (priv->proxy);
  sw_call_list_add (priv->calls, call);

  service = sw_item_view_get_service (SW_ITEM_VIEW (item_view));
  user_id = sw_service_lastfm_get_user_id (SW_SERVICE_LASTFM (service));

  rest_proxy_call_add_params (call,
                              "api_key", sw_keystore_get_key ("lastfm"),
                              "user", user_id,
                              "method", "user.getFriends",
                              NULL);
  rest_proxy_call_async (call,
                         _get_friends_cb,
                         (GObject *)item_view,
                         NULL,
                         NULL);
}


static gboolean
_update_timeout_cb (gpointer data)
{
  SwLastfmItemView *item_view = SW_LASTFM_ITEM_VIEW (data);

  _get_status_updates (item_view);

  return TRUE;
}

static void
_load_from_cache (SwLastfmItemView *item_view)
{
  SwLastfmItemViewPrivate *priv = GET_PRIVATE (item_view);
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
lastfm_item_view_start (SwItemView *item_view)
{
  SwLastfmItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (priv->timeout_id)
  {
    g_warning (G_STRLOC ": View already started.");
  } else {
    SW_DEBUG (TWITTER, G_STRLOC ": Setting up the timeout");
    priv->timeout_id = g_timeout_add_seconds (UPDATE_TIMEOUT,
                                              (GSourceFunc)_update_timeout_cb,
                                              item_view);

    _load_from_cache ((SwLastfmItemView *)item_view);
    _get_status_updates ((SwLastfmItemView *)item_view);
  }
}

static void
lastfm_item_view_stop (SwItemView *item_view)
{
  SwLastfmItemViewPrivate *priv = GET_PRIVATE (item_view);

  if (!priv->timeout_id)
  {
    g_warning (G_STRLOC ": View not running");
  } else {
    g_source_remove (priv->timeout_id);
    priv->timeout_id = 0;
  }
}

static void
lastfm_item_view_refresh (SwItemView *item_view)
{
  _get_status_updates ((SwLastfmItemView *)item_view);
}

static void
sw_lastfm_item_view_class_init (SwLastfmItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwItemViewClass *item_view_class = SW_ITEM_VIEW_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwLastfmItemViewPrivate));

  object_class->get_property = sw_lastfm_item_view_get_property;
  object_class->set_property = sw_lastfm_item_view_set_property;
  object_class->dispose = sw_lastfm_item_view_dispose;
  object_class->finalize = sw_lastfm_item_view_finalize;

  item_view_class->start = lastfm_item_view_start;
  item_view_class->stop = lastfm_item_view_stop;
  item_view_class->refresh = lastfm_item_view_refresh;

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
sw_lastfm_item_view_init (SwLastfmItemView *self)
{
  SwLastfmItemViewPrivate *priv = GET_PRIVATE (self);

  priv->calls = sw_call_list_new ();
  priv->set = sw_set_new ();
}


