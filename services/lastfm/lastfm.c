/*
 * libsocialweb - social data store
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
#include <gio/gio.h>
#include <libsocialweb/sw-service.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb/sw-call-list.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb-keystore/sw-keystore.h>
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <gconf/gconf-client.h>

#include "lastfm.h"
#include "lastfm-ginterface.h"

static void lastfm_iface_init (gpointer g_iface, gpointer iface_data);
static void initable_iface_init (gpointer g_iface, gpointer iface_data);
G_DEFINE_TYPE_WITH_CODE (SwServiceLastfm, sw_service_lastfm, SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                         G_IMPLEMENT_INTERFACE (SW_TYPE_LASTFM_IFACE, lastfm_iface_init));


#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_LASTFM, SwServiceLastfmPrivate))

#define KEY_BASE "/apps/libsocialweb/services/lastfm"
#define KEY_USER KEY_BASE "/user"

struct _SwServiceLastfmPrivate {
  gboolean running;
  RestProxy *proxy;
  GConfClient *gconf;
  char *user_id;
  guint gconf_notify_id;
  SwSet *set;
  SwCallList *calls;
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
lastfm_now_playing (SwLastfmIface *self,
                    const gchar *in_artist,
                    const gchar *in_album,
                    const gchar *in_track,
                    guint in_length,
                    guint in_tracknumber,
                    const gchar *in_musicbrainz,
                    DBusGMethodInvocation *context)
{
  /* TODO */
  sw_lastfm_iface_return_from_now_playing (context);
}

static void
lastfm_submit_track (SwLastfmIface *self,
                     const gchar *in_artist,
                     const gchar *in_album,
                     const gchar *in_track,
                     gint64 in_time,
                     const gchar *in_source,
                     const gchar *in_rating,
                     guint in_length,
                     guint in_tracknumber,
                     const gchar *in_musicbrainz,
                     DBusGMethodInvocation *context)
{
  /* TODO */
  sw_lastfm_iface_return_from_submit_track (context);
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
emit_if_done (SwServiceLastfm *lastfm)
{
  if (sw_call_list_is_empty (lastfm->priv->calls)) {
    SW_DEBUG (LASTFM, "Call set is empty, emitting refreshed signal");
    sw_service_emit_refreshed ((SwService *)lastfm, lastfm->priv->set);
    sw_set_empty (lastfm->priv->set);
  } else {
    SW_DEBUG (LASTFM, "Call set is not empty, still more work to do.");
  }
}

static void
get_artist_info_cb (RestProxyCall *call,
                    const GError  *error,
                    GObject       *weak_object,
                    gpointer       user_data)
{
  SwServiceLastfm *lastfm = SW_SERVICE_LASTFM (weak_object);
  SwItem *item = user_data;
  RestXmlNode *root, *artist_node;
  const char *url;

  sw_call_list_remove (lastfm->priv->calls, call);

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

  emit_if_done (lastfm);
}

static void
get_thumbnail (SwServiceLastfm *lastfm,
               SwItem          *item,
               RestXmlNode     *track_node)
{
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

  call = rest_proxy_new_call (lastfm->priv->proxy);
  sw_call_list_add (lastfm->priv->calls, call);

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

  rest_proxy_call_async (call, get_artist_info_cb, (GObject*)lastfm, item, NULL);
}

static void
start (SwService *service)
{
  SwServiceLastfm *lastfm = SW_SERVICE_LASTFM (service);

  SW_DEBUG (LASTFM, "Service started");
  lastfm->priv->running = TRUE;
}

static SwItem *
make_item (SwServiceLastfm *lastfm, RestXmlNode *user, RestXmlNode *track)
{
  RestXmlNode *date;
  SwItem *item;
  const char *s;
  char *id;

  item = sw_item_new ();
  sw_item_set_service (item, (SwService *)lastfm);

  id = g_strdup_printf ("%s %s",
                        rest_xml_node_find (track, "url")->content,
                        rest_xml_node_find (user, "name")->content);
  sw_item_take (item, "id", id);
  sw_item_put (item, "url", rest_xml_node_find (track, "url")->content);
  sw_item_take (item, "title", make_title (track));
  sw_item_put (item, "album", rest_xml_node_find (track, "album")->content);

  get_thumbnail (lastfm, item, track);

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
get_tracks_cb (RestProxyCall *call,
               const GError  *error,
               GObject       *weak_object,
               gpointer       user_data)
{
  SwServiceLastfm *lastfm = SW_SERVICE_LASTFM (weak_object);
  RestXmlNode *user_node = user_data;
  RestXmlNode *root, *track_node;

  sw_call_list_remove (lastfm->priv->calls, call);

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

  /* Although we only asked for a single track, if there is now-playing data
     then that is returned as well. */
  for (track_node = rest_xml_node_find (root, "track"); track_node; track_node = track_node->next) {
    SwItem *item;

    item = make_item (lastfm, user_node, track_node);
    sw_set_add (lastfm->priv->set, (GObject *)item);
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
  SwServiceLastfm *lastfm = SW_SERVICE_LASTFM (weak_object);
  RestXmlNode *root, *node;

  sw_call_list_remove (lastfm->priv->calls, call);

  if (error) {
    g_message (G_STRLOC ": error from Last.fm: %s", error->message);
    return;
  }

  SW_DEBUG (LASTFM, "Got result of getFriends call");

  root = node_from_call (call);
  if (!root)
    return;

  SW_DEBUG (LASTFM, "Parsed results of getFriends call");

  for (node = rest_xml_node_find (root, "user"); node; node = node->next) {
    call = rest_proxy_new_call (lastfm->priv->proxy);
    sw_call_list_add (lastfm->priv->calls, call);

    SW_DEBUG (LASTFM, "Making getRecentTracks call for %s",
                  rest_xml_node_find (node, "name")->content);

    rest_proxy_call_add_params (call,
                                "api_key", sw_keystore_get_key ("lastfm"),
                                "method", "user.getRecentTracks",
                                "user", rest_xml_node_find (node, "name")->content,
                                "limit", "1",
                                NULL);
    rest_proxy_call_async (call, get_tracks_cb, (GObject*)lastfm, rest_xml_node_ref (node), NULL);
  }
  rest_xml_node_unref (root);
}

static void
refresh (SwService *service)
{
  SwServiceLastfm *lastfm = SW_SERVICE_LASTFM (service);
  RestProxyCall *call;

  SW_DEBUG (LASTFM, "Refresh requested for instance %p", service);
  if (!lastfm->priv->running || lastfm->priv->user_id == NULL) {
    SW_DEBUG (LASTFM, "Refresh abandoned: running = %s, user_id = %s",
                  lastfm->priv->running ? "yes" : "no",
                  lastfm->priv->user_id);
    return;
  }

  sw_call_list_cancel_all (lastfm->priv->calls);
  sw_set_empty (lastfm->priv->set);

  SW_DEBUG (LASTFM, "Making getFriends call");
  call = rest_proxy_new_call (lastfm->priv->proxy);
  sw_call_list_add (lastfm->priv->calls, call);
  rest_proxy_call_add_params (call,
                              "api_key", sw_keystore_get_key ("lastfm"),
                              "user", lastfm->priv->user_id,
                              "method", "user.getFriends",
                              NULL);
  rest_proxy_call_async (call, get_friends_cb, (GObject*)service, NULL, NULL);
}

static void
user_changed_cb (GConfClient *client,
                 guint        cnxn_id,
                 GConfEntry  *entry,
                 gpointer     user_data)
{
  SwService *service = SW_SERVICE (user_data);
  SwServiceLastfm *lastfm = SW_SERVICE_LASTFM (service);
  SwServiceLastfmPrivate *priv = lastfm->priv;
  const char *new_user;

  SW_DEBUG (LASTFM, "User changed");
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

    SW_DEBUG (LASTFM, "User set to %s", priv->user_id);

    if (priv->user_id)
      refresh (service);
    else
      sw_service_emit_refreshed (service, NULL);
  }
}

static const char *
get_name (SwService *service)
{
  return "lastfm";
}

static void
sw_service_lastfm_dispose (GObject *object)
{
  SwServiceLastfmPrivate *priv = ((SwServiceLastfm*)object)->priv;

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
    sw_call_list_free (priv->calls);
    priv->calls = NULL;
  }

  if (priv->set) {
    sw_set_unref (priv->set);
    priv->set = NULL;
  }

  G_OBJECT_CLASS (sw_service_lastfm_parent_class)->dispose (object);
}

static void
sw_service_lastfm_finalize (GObject *object)
{
  SwServiceLastfmPrivate *priv = ((SwServiceLastfm*)object)->priv;

  g_free (priv->user_id);

  G_OBJECT_CLASS (sw_service_lastfm_parent_class)->finalize (object);
}

static gboolean
sw_service_lastfm_initable (GInitable     *initable,
                            GCancellable  *cancellable,
                            GError       **error)
{
  SwServiceLastfm *lastfm = SW_SERVICE_LASTFM (initable);
  SwServiceLastfmPrivate *priv = lastfm->priv;

  SW_DEBUG (LASTFM, "%s called", G_STRFUNC);
  if (sw_keystore_get_key ("lastfm") == NULL) {
    g_set_error_literal (error,
                         SW_SERVICE_ERROR,
                         SW_SERVICE_ERROR_NO_KEYS,
                         "No API key configured");
    return FALSE;
  }

  if (priv->proxy)
    return TRUE;

  priv->set = sw_item_set_new ();
  priv->calls = sw_call_list_new ();

  priv->running = FALSE;

  priv->proxy = rest_proxy_new ("http://ws.audioscrobbler.com/2.0/", FALSE);

  priv->gconf = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf, KEY_BASE,
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  priv->gconf_notify_id = gconf_client_notify_add (priv->gconf, KEY_USER,
                                                   user_changed_cb, lastfm,
                                                   NULL, NULL);
  gconf_client_notify (priv->gconf, KEY_USER);

  return TRUE;
}

static void
initable_iface_init (gpointer g_iface,
                     gpointer iface_data)
{
  GInitableIface *klass = (GInitableIface *)g_iface;

  klass->init = sw_service_lastfm_initable;
}

static void
lastfm_iface_init (gpointer g_iface,
                   gpointer iface_data)
{
  SwLastfmIfaceClass *klass = (SwLastfmIfaceClass *)g_iface;

  sw_lastfm_iface_implement_submit_track (klass, lastfm_submit_track);
  sw_lastfm_iface_implement_now_playing (klass, lastfm_now_playing);
}

static void
sw_service_lastfm_class_init (SwServiceLastfmClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceLastfmPrivate));

  object_class->dispose = sw_service_lastfm_dispose;
  object_class->finalize = sw_service_lastfm_finalize;

  service_class->get_name = get_name;
  service_class->start = start;
  service_class->refresh = refresh;
}

static void
sw_service_lastfm_init (SwServiceLastfm *self)
{
  self->priv = GET_PRIVATE (self);
}


const gchar *
sw_service_lastfm_get_user_id (SwServiceLastfm *service)
{
  return service->priv->user_id;
}
