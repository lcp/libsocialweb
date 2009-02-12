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
#include <mojito/mojito-service.h>
#include <mojito/mojito-item.h>
#include <mojito/mojito-utils.h>
#include <mojito/mojito-web.h>
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include <gconf/gconf-client.h>

#include "lastfm.h"

G_DEFINE_TYPE (MojitoServiceLastfm, mojito_service_lastfm, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_LASTFM, MojitoServiceLastfmPrivate))

/* TODO: get proper API key */
#define API_KEY "aa581f6505fd3ea79073ddcc2215cbc7"

#define KEY_BASE "/apps/mojito/services/lastfm"
#define KEY_USER KEY_BASE "/user"

struct _MojitoServiceLastfmPrivate {
  RestProxy *proxy;
  GConfClient *gconf;
  char *user_id;
};

static void
user_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  MojitoServiceLastfm *lastfm = MOJITO_SERVICE_LASTFM (user_data);
  MojitoServiceLastfmPrivate *priv = lastfm->priv;

  g_free (priv->user_id);
  priv->user_id = g_strdup (gconf_value_get_string (entry->value));
}

static char *
make_title (RestXmlNode *node)
{
  const char *track, *artist;

  track = rest_xml_node_find (node, "name")->content;
  artist = rest_xml_node_find (node, "artist")->content;

  if (track && artist) {
    return g_strdup_printf ("%s by %s", track, artist);
  } else if (track) {
    return g_strdup (track);
  } else {
    return g_strdup ("Unknown");
  }
}

static char *
get_image (RestXmlNode *node, const char *size)
{
  g_assert (node);
  g_assert (size);

  for (node = rest_xml_node_find (node, "image"); node; node = node->next) {
    /* Skip over images which are not medium sized */
    if (!g_str_equal (rest_xml_node_get_attr (node, "size"), size))
      continue;

    if (node->content) {
      return mojito_web_download_image (node->content);
    } else {
      /* TODO: should fetch another size instead */
      return NULL;
    }
  }

  return NULL;
}

/*
 * Run the call, displaying any errors as appropriate, and return the parsed
 * document if it succeeded.
 */
static RestXmlNode *
lastfm_call (RestProxyCall *call)
{
  GError *error = NULL;
  RestXmlParser *parser;
  RestXmlNode *node;

  g_assert (call);

  rest_proxy_call_run (call, NULL, &error);
  parser = rest_xml_parser_new ();
  node = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  /* No content, or wrong content */
  if (node == NULL || strcmp (node->name, "lfm") != 0) {
    g_printerr ("Cannot make Last.fm call: %s\n",
                error ? error->message : "unknown reason");
    if (error) g_error_free (error);
    if (node) rest_xml_node_unref (node);
    return NULL;
  }

  if (strcmp (rest_xml_node_get_attr (node, "status"), "ok") != 0) {
    RestXmlNode *node2;
    node2 = rest_xml_node_find (node, "error");
    g_printerr ("Cannot make Last.fm call: %s (code %s)\n",
                node2->content,
                rest_xml_node_get_attr (node2, "code"));
    rest_xml_node_unref (node);
    return NULL;
  }

  return node;
}

static void
update (MojitoService *service, MojitoServiceDataFunc callback, gpointer user_data)
{
  MojitoServiceLastfm *lastfm = MOJITO_SERVICE_LASTFM (service);
  RestProxyCall *call;
  RestXmlNode *root, *node;
  MojitoSet *set;

  if (lastfm->priv->user_id == NULL) {
    callback (service, NULL, user_data);
    return;
  }

  call = rest_proxy_new_call (lastfm->priv->proxy);
  rest_proxy_call_add_params (call,
                              "api_key", API_KEY,
                              "method", "user.getFriends",
                              "user", lastfm->priv->user_id,
                              NULL);

  if ((root = lastfm_call (call)) == NULL) {
    callback (service, NULL, user_data);
    return;
  }

  set = mojito_item_set_new ();

  for (node = rest_xml_node_find (root, "user"); node; node = node->next) {
    MojitoItem *item;
    RestXmlNode *recent, *track, *date;
    const char *s;

    call = rest_proxy_new_call (lastfm->priv->proxy);
    rest_proxy_call_add_params (call,
                                "api_key", API_KEY,
                                "method", "user.getRecentTracks",
                                "user", rest_xml_node_find (node, "name")->content,
                                "limit", "1",
                                NULL);

    if ((recent = lastfm_call (call)) == NULL) {
      continue;
    }

    track = rest_xml_node_find (recent, "track");

    if (!track) {
      rest_xml_node_unref (recent);
      continue;
    }

    item = mojito_item_new ();
    mojito_item_set_service (item, service);

    /* TODO user+track url? user+timestamp? */
    mojito_item_put (item, "id", rest_xml_node_find (track, "url")->content);
    mojito_item_put (item, "link", rest_xml_node_find (track, "url")->content);
    mojito_item_take (item, "title", make_title (track));
    mojito_item_put (item, "album", rest_xml_node_find (track, "album")->content);

    mojito_item_take (item, "thumbnail", get_image (track, "large"));

    date = rest_xml_node_find (track, "date");
    mojito_item_take (item, "date", mojito_time_t_to_string (atoi (rest_xml_node_get_attr (date, "uts"))));

    s = rest_xml_node_find (node, "realname")->content;
    if (s) mojito_item_put (item, "author", s);
    mojito_item_put (item, "authorid", rest_xml_node_find (node, "name")->content);
    mojito_item_take (item, "authoricon", get_image (node, "medium"));

    rest_xml_node_unref (recent);

    mojito_set_add (set, (GObject*)item);
  }

  rest_xml_node_unref (root);

  callback (service, set, user_data);
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
    g_object_unref (priv->gconf);
    priv->gconf = NULL;
  }

  G_OBJECT_CLASS (mojito_service_lastfm_parent_class)->dispose (object);
}

static void
mojito_service_lastfm_finalize (GObject *object)
{
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
  service_class->update = update;
}

static void
mojito_service_lastfm_init (MojitoServiceLastfm *self)
{
  MojitoServiceLastfmPrivate *priv;

  priv = self->priv = GET_PRIVATE (self);

  priv->proxy = rest_proxy_new ("http://ws.audioscrobbler.com/2.0/", FALSE);

  priv->gconf = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf, KEY_BASE,
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_notify_add (priv->gconf, KEY_USER,
                           user_changed_cb, self, NULL, NULL);
  gconf_client_notify (priv->gconf, KEY_USER);
}
