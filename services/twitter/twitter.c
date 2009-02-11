/*
 * Mojito - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
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

#include <stdlib.h>
#include "twitter.h"
#include <twitter-glib/twitter-glib.h>
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>
#include <mojito/mojito-utils.h>
#include <mojito/mojito-web.h>
#include <gconf/gconf-client.h>
#include <libsoup/soup.h>

G_DEFINE_TYPE (MojitoServiceTwitter, mojito_service_twitter, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_TWITTER, MojitoServiceTwitterPrivate))

#define KEY_DIR "/apps/mojito/services/twitter"
#define KEY_USER KEY_DIR "/user"
#define KEY_PASSWORD KEY_DIR "/password"

struct _MojitoServiceTwitterPrivate {
  SoupSession *soup;
  GConfClient *gconf;
  gboolean user_set, password_set;

  TwitterClient *client;

  /* This is grim */
  MojitoSet *set;
  MojitoServiceDataFunc callback;
  gpointer user_data;
};

static void
user_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  MojitoServiceTwitter *twitter = MOJITO_SERVICE_TWITTER (user_data);
  MojitoServiceTwitterPrivate *priv = twitter->priv;
  const char *s = NULL;

  if (g_str_equal (entry->key, KEY_USER)) {
    if (entry->value)
      s = gconf_value_get_string (entry->value);
    g_object_set (priv->client, "email", s, NULL);
    priv->user_set = (s && s[0] != '\0');
  } else if (g_str_equal (entry->key, KEY_PASSWORD)) {
    if (entry->value)
      s = gconf_value_get_string (entry->value);
    g_object_set (priv->client, "password", s, NULL);
    priv->password_set = (s && s[0] != '\0');
  }
}

static void
status_received_cb (TwitterClient *client,
                    TwitterStatus *status,
                    const GError  *error,
                    gpointer       user_data)
{
  MojitoServiceTwitter *service = MOJITO_SERVICE_TWITTER (user_data);
  TwitterUser *user;
  MojitoItem *item;
  char *date;
  GTimeVal timeval;

  if (error) {
    g_debug ("Cannot update Twitter: %s", error->message);
    service->priv->callback ((MojitoService*)service, NULL, service->priv->user_data);
    return;
  }

  item = mojito_item_new ();
  mojito_item_set_service (item, MOJITO_SERVICE (service));

  user = twitter_status_get_user (status);

  twitter_date_to_time_val (twitter_status_get_created_at (status), &timeval);
  date = mojito_time_t_to_string (timeval.tv_sec);

  mojito_item_put (item, "id", twitter_status_get_url (status));
  mojito_item_put (item, "url", twitter_status_get_url (status));
  mojito_item_take (item, "date", date);
  /* TODO: need a better name for this */
  mojito_item_put (item, "content", twitter_status_get_text (status));

  mojito_item_put (item, "author", twitter_user_get_name (user));
  mojito_item_take (item, "authorid", g_strdup_printf ("%d", twitter_user_get_id (user)));
  mojito_item_take (item, "authoricon", mojito_web_download_image
                    (service->priv->soup, twitter_user_get_profile_image_url (user)));

  mojito_set_add (service->priv->set, (GObject*)item);
  g_object_unref (item);
}

static void
timeline_received_cb (TwitterClient *client,
                      gpointer       user_data)
{
  MojitoServiceTwitter *service = MOJITO_SERVICE_TWITTER (user_data);

  service->priv->callback ((MojitoService*)service, mojito_set_ref (service->priv->set), service->priv->user_data);
}

static void
update (MojitoService *service, MojitoServiceDataFunc callback, gpointer user_data)
{
  MojitoServiceTwitter *twitter = (MojitoServiceTwitter*)service;
  MojitoServiceTwitterPrivate *priv = twitter->priv;

  if (!priv->user_set || !priv->password_set) {
    callback (service, NULL, user_data);
    return;
  }

  /* TODO grim */
  twitter->priv->callback = callback;
  twitter->priv->user_data = user_data;
  mojito_set_empty (twitter->priv->set);

  twitter_client_get_friends_timeline (twitter->priv->client, NULL, 0);
}

static const char *
mojito_service_twitter_get_name (MojitoService *service)
{
  return "twitter";
}

static void
mojito_service_twitter_dispose (GObject *object)
{
  MojitoServiceTwitterPrivate *priv = MOJITO_SERVICE_TWITTER (object)->priv;

  if (priv->soup) {
    g_object_unref (priv->soup);
    priv->soup = NULL;
  }

  if (priv->gconf) {
    g_object_unref (priv->gconf);
    priv->gconf = NULL;
  }

  if (priv->client) {
    g_object_unref (priv->client);
    priv->client = NULL;
  }

  G_OBJECT_CLASS (mojito_service_twitter_parent_class)->dispose (object);
}

static void
mojito_service_twitter_finalize (GObject *object)
{
  //MojitoServiceTwitterPrivate *priv = MOJITO_SERVICE_TWITTER (object)->priv;

  G_OBJECT_CLASS (mojito_service_twitter_parent_class)->finalize (object);
}

static void
mojito_service_twitter_class_init (MojitoServiceTwitterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoServiceClass *service_class = MOJITO_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoServiceTwitterPrivate));

  object_class->dispose = mojito_service_twitter_dispose;
  object_class->finalize = mojito_service_twitter_finalize;

  service_class->get_name = mojito_service_twitter_get_name;
  service_class->update = update;
}

static void
mojito_service_twitter_init (MojitoServiceTwitter *self)
{
  MojitoServiceTwitterPrivate *priv;

  self->priv = priv = GET_PRIVATE (self);

  priv->user_set = priv->password_set = FALSE;

  /* TODO: when the image fetching is async change this to async */
  priv->soup = mojito_web_make_sync_session ();

  priv->gconf = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf, KEY_DIR,
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_notify_add (priv->gconf, KEY_USER,
                           user_changed_cb, self, NULL, NULL);
  gconf_client_notify_add (priv->gconf, KEY_PASSWORD,
                           user_changed_cb, self, NULL, NULL);

  /* TODO: set user agent */
  priv->client = twitter_client_new ();
  g_signal_connect (priv->client, "status-received",
                    G_CALLBACK (status_received_cb),
                    self);
  g_signal_connect (priv->client, "timeline-complete",
                    G_CALLBACK (timeline_received_cb),
                    self);

  priv->set = mojito_item_set_new ();

  /* Load preferences */
  gconf_client_notify (priv->gconf, KEY_USER);
  gconf_client_notify (priv->gconf, KEY_PASSWORD);
}
