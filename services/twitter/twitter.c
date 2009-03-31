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

#include <config.h>
#include <stdlib.h>
#include "twitter.h"
#include <twitter-glib/twitter-glib.h>
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>
#include <mojito/mojito-utils.h>
#include <mojito/mojito-web.h>
#include <gconf/gconf-client.h>

G_DEFINE_TYPE (MojitoServiceTwitter, mojito_service_twitter, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_TWITTER, MojitoServiceTwitterPrivate))

#define KEY_DIR "/apps/mojito/services/twitter"
#define KEY_USER KEY_DIR "/user"
#define KEY_PASSWORD KEY_DIR "/password"

struct _MojitoServiceTwitterPrivate {
  GConfClient *gconf;
  gboolean user_set, password_set;
  gchar *username;

  TwitterClient *client;
  TwitterUser *user;

  gulong self_handle;

  MojitoSet *set;
};

static void
user_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  MojitoServiceTwitter *twitter = MOJITO_SERVICE_TWITTER (user_data);
  MojitoServiceTwitterPrivate *priv = twitter->priv;
  const char *username = NULL, *password = NULL;

  if (g_str_equal (entry->key, KEY_USER)) {
    if (entry->value)
      username = gconf_value_get_string (entry->value);
    g_object_set (priv->client, "email", username, NULL);
    priv->user_set = (username && username[0] != '\0');
    priv->username = g_strdup (username);
  } else if (g_str_equal (entry->key, KEY_PASSWORD)) {
    if (entry->value)
      password = gconf_value_get_string (entry->value);
    g_object_set (priv->client, "password", password, NULL);
    priv->password_set = (password && password[0] != '\0');
  }

  /* Get our details. This will cause the user_received signal to be fired */
  if (priv->user_set && priv->password_set) {
    /* Despite claiming to take an email it also takes a username */
    priv->self_handle =
      twitter_client_show_user_from_email (priv->client, priv->username);
  }
}

static MojitoItem *
make_item_from_status (MojitoService *service, TwitterStatus *status)
{
  MojitoItem *item;
  TwitterUser *user;
  GTimeVal timeval;
  char *date;

  g_assert (MOJITO_IS_SERVICE (service));
  g_assert (TWITTER_IS_STATUS (status));

  item = mojito_item_new ();
  mojito_item_set_service (item, MOJITO_SERVICE (service));

  twitter_date_to_time_val (twitter_status_get_created_at (status), &timeval);
  date = mojito_time_t_to_string (timeval.tv_sec);

  mojito_item_put (item, "id", twitter_status_get_url (status));
  mojito_item_put (item, "url", twitter_status_get_url (status));
  mojito_item_take (item, "date", date);
  /* TODO: need a better name for this */
  mojito_item_put (item, "content", twitter_status_get_text (status));

  user = twitter_status_get_user (status);
  mojito_item_put (item, "author", twitter_user_get_name (user));
  mojito_item_take (item, "authorid", g_strdup_printf ("%d", twitter_user_get_id (user)));
  mojito_item_take (item, "authoricon", mojito_web_download_image
                    (twitter_user_get_profile_image_url (user)));

  return item;
}

static void
status_received_cb (TwitterClient *client,
                    gulong         handle,
                    TwitterStatus *status,
                    const GError  *error,
                    gpointer       user_data)
{
  MojitoServiceTwitter *service = MOJITO_SERVICE_TWITTER (user_data);
  MojitoItem *item;

  if (error) {
    g_message ("Cannot update Twitter: %s", error->message);
    return;
  }

  item = make_item_from_status ((MojitoService*)service, status);
  mojito_set_add (service->priv->set, (GObject*)item);
  g_object_unref (item);
}

static void
timeline_received_cb (TwitterClient *client,
                      gpointer       user_data)
{
  MojitoService *service = MOJITO_SERVICE (user_data);
  MojitoServiceTwitterPrivate *priv = GET_PRIVATE (service);

  mojito_service_emit_refreshed (service, priv->set);
  mojito_set_empty (priv->set);
}

static void
user_received_cb (TwitterClient *client,
                  gulong         handle,
                  TwitterUser   *user,
                  const GError  *error,
                  gpointer       userdata)
{
  MojitoServiceTwitter *service = (MojitoServiceTwitter *)userdata;
  MojitoServiceTwitterPrivate *priv = GET_PRIVATE (service);

  if (!user) {
    g_warning (G_STRLOC ": Error when getting user information: %s",
               error->message);
    return;
  }

  /* Check that this is us. Not somebody else */
  if (priv->self_handle == handle ||
      g_str_equal (twitter_user_get_screen_name (user), priv->username)) {
    priv->user = g_object_ref (user);
  }
}

static void
refresh (MojitoService *service)
{
  MojitoServiceTwitter *twitter = (MojitoServiceTwitter*)service;
  MojitoServiceTwitterPrivate *priv = twitter->priv;
  GHashTable *params = NULL;

  if (!priv->user_set || !priv->password_set)
    return;

  mojito_set_empty (priv->set);

  g_object_get (service, "params", &params, NULL);

  if (params && g_hash_table_lookup (params, "own")) {
    twitter_client_get_user_timeline (priv->client, priv->username, 0, 0);
  } else {
    twitter_client_get_friends_timeline (priv->client, NULL, 0);
  }

  if (params)
    g_hash_table_unref (params);
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

  if (priv->gconf) {
    g_object_unref (priv->gconf);
    priv->gconf = NULL;
  }

  if (priv->user) {
    g_object_unref (priv->user);
    priv->user = NULL;
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
  MojitoServiceTwitterPrivate *priv = MOJITO_SERVICE_TWITTER (object)->priv;

  g_free (priv->username);
  G_OBJECT_CLASS (mojito_service_twitter_parent_class)->finalize (object);
}

static gboolean
update_status (MojitoService *service,
               const gchar   *status_msg)
{
  MojitoServiceTwitterPrivate *priv = GET_PRIVATE (service);

  twitter_client_add_status (priv->client, status_msg);

  return TRUE;
}

static guint32
get_capabilities (MojitoService *service)
{
  return SERVICE_CAN_UPDATE_STATUS |
    SERVICE_CAN_GET_PERSONA_ICON;
}

static gchar *
get_persona_icon (MojitoService *service)
{
  MojitoServiceTwitterPrivate *priv = GET_PRIVATE (service);
  const gchar *url;

  if (priv->user) {
    url = twitter_user_get_profile_image_url (priv->user);
    return mojito_web_download_image (url);
  } else {
    return NULL;
  }
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
  service_class->refresh = refresh;
  service_class->get_capabilities = get_capabilities;
  service_class->update_status = update_status;
  service_class->get_persona_icon = get_persona_icon;
}

static void
mojito_service_twitter_init (MojitoServiceTwitter *self)
{
  MojitoServiceTwitterPrivate *priv;

  self->priv = priv = GET_PRIVATE (self);

  priv->user_set = priv->password_set = FALSE;

  priv->gconf = gconf_client_get_default ();
  gconf_client_add_dir (priv->gconf, KEY_DIR,
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_notify_add (priv->gconf, KEY_USER,
                           user_changed_cb, self, NULL, NULL);
  gconf_client_notify_add (priv->gconf, KEY_PASSWORD,
                           user_changed_cb, self, NULL, NULL);

  priv->client = g_object_new (TWITTER_TYPE_CLIENT,
                               "user-agent", "Mojito/" VERSION,
                               NULL);
  g_signal_connect (priv->client, "status-received",
                    G_CALLBACK (status_received_cb),
                    self);
  g_signal_connect (priv->client, "timeline-complete",
                    G_CALLBACK (timeline_received_cb),
                    self);
  g_signal_connect (priv->client, "user-received",
                    G_CALLBACK (user_received_cb),
                    self);

  priv->set = mojito_item_set_new ();

  /* Load preferences */
  gconf_client_notify (priv->gconf, KEY_USER);
  gconf_client_notify (priv->gconf, KEY_PASSWORD);
}
