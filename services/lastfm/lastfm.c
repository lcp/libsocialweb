/*
 * Mojito - social data store
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
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <mojito/mojito-service.h>
#include <mojito/mojito-item.h>
#include <mojito/mojito-utils.h>
#include <mojito/mojito-web.h>
#include <mojito/mojito-call-list.h>
#include <mojito-keystore/mojito-keystore.h>
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <gconf/gconf-client.h>

#include "lastfm.h"

G_DEFINE_TYPE (MojitoServiceLastfm, mojito_service_lastfm, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_LASTFM, MojitoServiceLastfmPrivate))

#define KEY_BASE "/apps/mojito/services/lastfm"
#define KEY_USER KEY_BASE "/user"

struct _MojitoServiceLastfmPrivate {
  gboolean running;
  RestProxy *proxy;
  GConfClient *gconf;
  char *user_id;
  guint gconf_notify_id;
  MojitoSet *set;
  MojitoCallList *calls;
};

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
    g_message ("Error from Last.fm: %s (%d)",
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
    g_printerr ("Cannot make Last.fm call: %s\n",
                error ? error->message : "unknown reason");
    if (error) g_error_free (error);
    if (node) rest_xml_node_unref (node);
    return NULL;
  }

  if (strcmp (rest_xml_node_get_attr (node, "status"), "ok") != 0) {
    RestXmlNode *err_node;
    err_node = rest_xml_node_find (node, "error");
    g_printerr ("Cannot make Last.fm call: %s (code %s)\n",
                err_node->content,
                rest_xml_node_get_attr (err_node, "code"));
    rest_xml_node_unref (node);
    return NULL;
  }

  return node;
}

static char *
make_title (RestXmlNode *node)
{
  const char *track, *artist;

  track = rest_xml_node_find (node, "name")->content;
  artist = rest_xml_node_find (node, "artist")->content;

  if (track && artist) {
    return g_strdup_printf (_("%s by %s"), track, artist);
  } else if (track) {
    return g_strdup (track);
  } else {
    return g_strdup (_("Unknown"));
  }
}

static const char *
get_image_url (RestXmlNode *node, const char *size)
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
emit_if_done (MojitoServiceLastfm *lastfm)
{
  if (mojito_call_list_is_empty (lastfm->priv->calls)) {
    mojito_service_emit_refreshed ((MojitoService *)lastfm, lastfm->priv->set);
    mojito_set_empty (lastfm->priv->set);
  }
}

static void
get_artist_info_cb (RestProxyCall *call,
                    const GError  *error,
                    GObject       *weak_object,
                    gpointer       user_data)
{
  MojitoServiceLastfm *lastfm = MOJITO_SERVICE_LASTFM (weak_object);
  MojitoItem *item = user_data;
  RestXmlNode *root, *artist_node;
  const char *url;

  mojito_call_list_remove (lastfm->priv->calls, call);

  if (error) {
    g_message ("Error: %s", error->message);
    /* TODO: cleanup or something */
    return;
  }

  root = node_from_call (call);
  if (!root)
    return;

  artist_node = rest_xml_node_find (root, "artist");
  url = get_image_url (artist_node, "large");
  if (url)
    mojito_item_request_image_fetch (item, "thumbnail", url);

  mojito_item_pop_pending (item);

  emit_if_done (lastfm);
}

static void
get_thumbnail (MojitoServiceLastfm *lastfm, MojitoItem *item, RestXmlNode *track_node)
{
  const char *url;
  RestProxyCall *call;
  RestXmlNode *artist;
  const char *mbid;

  url = get_image_url (track_node, "large");
  if (url) {
    mojito_item_request_image_fetch (item, "thumbnail", url);
    return;
  }

  /* If we didn't find an album image, then try the artist image */

  mojito_item_push_pending (item);

  call = rest_proxy_new_call (lastfm->priv->proxy);
  mojito_call_list_add (lastfm->priv->calls, call);

  rest_proxy_call_add_params (call,
                              "method", "artist.getInfo",
                              "api_key", mojito_keystore_get_key ("lastfm"),
                              NULL);

  artist = rest_xml_node_find (track_node, "artist");
  mbid = rest_xml_node_get_attr (artist, "mbid");
  if (mbid && mbid[0] != '\0') {
    rest_proxy_call_add_param (call, "mbid", mbid);
  } else {
    rest_proxy_call_add_param (call, "artist", artist->content);
  }

  rest_proxy_call_async (call, get_artist_info_cb, (GObject*)lastfm, item, NULL);
}

static void
start (MojitoService *service)
{
  MojitoServiceLastfm *lastfm = MOJITO_SERVICE_LASTFM (service);

  lastfm->priv->running = TRUE;
}

static MojitoItem *
make_item (MojitoServiceLastfm *lastfm, RestXmlNode *user, RestXmlNode *track)
{
  RestXmlNode *date;
  MojitoItem *item;
  const char *s;
  char *id;

  item = mojito_item_new ();
  mojito_item_set_service (item, (MojitoService *)lastfm);

  id = g_strdup_printf ("%s %s",
                        rest_xml_node_find (track, "url")->content,
                        rest_xml_node_find (user, "name")->content);
  mojito_item_take (item, "id", id);
  mojito_item_put (item, "url", rest_xml_node_find (track, "url")->content);
  mojito_item_take (item, "title", make_title (track));
  mojito_item_put (item, "album", rest_xml_node_find (track, "album")->content);

  get_thumbnail (lastfm, item, track);

  date = rest_xml_node_find (track, "date");
  mojito_item_take (item, "date", mojito_time_t_to_string (atoi (rest_xml_node_get_attr (date, "uts"))));

  s = rest_xml_node_find (user, "realname")->content;
  if (s) {
    mojito_item_put (item, "author", s);
  } else {
    mojito_item_put (item, "author", rest_xml_node_find (user, "name")->content);
  }

  mojito_item_put (item, "authorid", rest_xml_node_find (user, "name")->content);

  s = get_image_url (user, "medium");
  if (s)
    mojito_item_request_image_fetch (item, "authoricon", s);

  return item;
}

static void
get_tracks_cb (RestProxyCall *call,
               const GError  *error,
               GObject       *weak_object,
               gpointer       user_data)
{
  MojitoServiceLastfm *lastfm = MOJITO_SERVICE_LASTFM (weak_object);
  RestXmlNode *user_node = user_data;
  RestXmlNode *root, *track_node;

  mojito_call_list_remove (lastfm->priv->calls, call);

  if (error) {
    g_message ("Error: %s", error->message);
    /* TODO: cleanup or something */
    return;
  }

  root = node_from_call (call);
  if (!root)
    return;

  track_node = rest_xml_node_find (root, "track");
  if (track_node) {
    MojitoItem *item;

    item = make_item (lastfm, user_node, track_node);
    mojito_set_add (lastfm->priv->set, (GObject *)item);
  }

  rest_xml_node_unref (root);

  emit_if_done (lastfm);
}

static void
get_friends_cb (RestProxyCall *call,
                const GError  *error,
                GObject       *weak_object,
                gpointer       user_data)
{
  MojitoServiceLastfm *lastfm = MOJITO_SERVICE_LASTFM (weak_object);
  RestXmlNode *root, *node;

  mojito_call_list_remove (lastfm->priv->calls, call);

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  root = node_from_call (call);
  if (!root)
    return;

  for (node = rest_xml_node_find (root, "user"); node; node = node->next) {
    call = rest_proxy_new_call (lastfm->priv->proxy);
    mojito_call_list_add (lastfm->priv->calls, call);

    rest_proxy_call_add_params (call,
                                "api_key", mojito_keystore_get_key ("lastfm"),
                                "method", "user.getRecentTracks",
                                "user", rest_xml_node_find (node, "name")->content,
                                "limit", "1",
                                NULL);
    rest_proxy_call_async (call, get_tracks_cb, (GObject*)lastfm, rest_xml_node_ref (node), NULL);
  }
  rest_xml_node_unref (root);
}

static void
refresh (MojitoService *service)
{
  MojitoServiceLastfm *lastfm = MOJITO_SERVICE_LASTFM (service);
  RestProxyCall *call;

  if (!lastfm->priv->running || lastfm->priv->user_id == NULL) {
    return;
  }

  mojito_call_list_cancel_all (lastfm->priv->calls);
  mojito_set_empty (lastfm->priv->set);

  call = rest_proxy_new_call (lastfm->priv->proxy);
  mojito_call_list_add (lastfm->priv->calls, call);
  rest_proxy_call_add_params (call,
                              "api_key", mojito_keystore_get_key ("lastfm"),
                              "user", lastfm->priv->user_id,
                              "method", "user.getFriends",
                              NULL);
  rest_proxy_call_async (call, get_friends_cb, (GObject*)service, NULL, NULL);
}

static void
user_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  MojitoService *service = MOJITO_SERVICE (user_data);
  MojitoServiceLastfm *lastfm = MOJITO_SERVICE_LASTFM (service);
  MojitoServiceLastfmPrivate *priv = lastfm->priv;
  const char *new_user;

  if (entry->value) {
    new_user = gconf_value_get_string (entry->value);
    if (new_user && new_user[0] == '\0')
      new_user = NULL;
  } else {
    new_user = NULL;
  }

  if (g_strcmp0 (new_user, priv->user_id) != 0) {
    g_free (priv->user_id);
    priv->user_id = g_strdup (new_user);

    if (priv->user_id)
      refresh (service);
    else
      mojito_service_emit_refreshed (service, NULL);
  }
}

static const char *
get_name (MojitoService *service)
{
  return "lastfm";
}

static void
mojito_service_lastfm_dispose (GObject *object)
{
  MojitoServiceLastfmPrivate *priv = ((MojitoServiceLastfm*)object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  if (priv->gconf) {
    gconf_client_notify_remove (priv->gconf,
                                priv->gconf_notify_id);
    gconf_client_remove_dir (priv->gconf, KEY_BASE, NULL);
    g_object_unref (priv->gconf);
    priv->gconf = NULL;
  }

  /* Do this here so only disposing if there are callbacks pending */
  if (priv->calls) {
    mojito_call_list_free (priv->calls);
    priv->calls = NULL;
  }

  G_OBJECT_CLASS (mojito_service_lastfm_parent_class)->dispose (object);
}

static void
mojito_service_lastfm_finalize (GObject *object)
{
  MojitoServiceLastfmPrivate *priv = ((MojitoServiceLastfm*)object)->priv;

  g_free (priv->user_id);

  mojito_set_unref (priv->set);

  G_OBJECT_CLASS (mojito_service_lastfm_parent_class)->finalize (object);
}

static void
mojito_service_lastfm_class_init (MojitoServiceLastfmClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoServiceClass *service_class = MOJITO_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoServiceLastfmPrivate));

  object_class->dispose = mojito_service_lastfm_dispose;
  object_class->finalize = mojito_service_lastfm_finalize;

  service_class->get_name = get_name;
  service_class->start = start;
  service_class->refresh = refresh;
}

static void
mojito_service_lastfm_init (MojitoServiceLastfm *self)
{
  MojitoServiceLastfmPrivate *priv;

  priv = self->priv = GET_PRIVATE (self);

  priv->set = mojito_item_set_new ();
  priv->calls = mojito_call_list_new ();

  priv->running = FALSE;

  priv->proxy = rest_proxy_new ("http://ws.audioscrobbler.com/2.0/", FALSE);

  priv->gconf = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf, KEY_BASE,
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  priv->gconf_notify_id = gconf_client_notify_add (priv->gconf, KEY_USER,
                                                   user_changed_cb, self,
                                                   NULL, NULL);
  gconf_client_notify (priv->gconf, KEY_USER);
}
