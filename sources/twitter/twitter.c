#include <stdlib.h>
#include "twitter.h"
#include <twitter-glib/twitter-glib.h>
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>
#include <mojito/mojito-utils.h>
#include <mojito/mojito-web.h>
#include <gconf/gconf-client.h>
#include <libsoup/soup.h>

G_DEFINE_TYPE (MojitoSourceTwitter, mojito_source_twitter, MOJITO_TYPE_SOURCE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SOURCE_TWITTER, MojitoSourceTwitterPrivate))

#define KEY_DIR "/apps/mojito/sources/twitter"
#define KEY_USER KEY_DIR "/user"
#define KEY_PASSWORD KEY_DIR "/password"

struct _MojitoSourceTwitterPrivate {
  SoupSession *soup;
  GConfClient *gconf;
  gboolean user_set, password_set;

  TwitterClient *client;

  /* This is grim */
  MojitoSet *set;
  MojitoSourceDataFunc callback;
  gpointer user_data;
};

static void
user_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  MojitoSourceTwitter *twitter = MOJITO_SOURCE_TWITTER (user_data);
  MojitoSourceTwitterPrivate *priv = twitter->priv;
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
  MojitoSourceTwitter *source = MOJITO_SOURCE_TWITTER (user_data);
  TwitterUser *user;
  MojitoItem *item;
  char *url, *date;
  GTimeVal timeval;

  if (error) {
    g_debug ("Cannot update Twitter: %s", error->message);
    source->priv->callback ((MojitoSource*)source, NULL, source->priv->user_data);
    return;
  }

  item = mojito_item_new ();
  mojito_item_set_source (item, MOJITO_SOURCE (source));

  user = twitter_status_get_user (status);

  url = g_strdup_printf ("http://twitter.com/%s/statuses/%d",
                         twitter_user_get_screen_name (user),
                         twitter_status_get_id (status));

  twitter_date_to_time_val (twitter_status_get_created_at (status), &timeval);
  date = mojito_time_t_to_string (timeval.tv_sec);

  mojito_item_put (item, "id", url);
  mojito_item_put (item, "url", url);
  mojito_item_take (item, "date", date);
  /* TODO: need a better name for this */
  mojito_item_put (item, "content", twitter_status_get_text (status));

  mojito_item_put (item, "author", twitter_user_get_name (user));
  mojito_item_take (item, "authorid", g_strdup_printf ("%d", twitter_user_get_id (user)));
  mojito_item_take (item, "authoricon", mojito_web_download_image
                    (source->priv->soup, twitter_user_get_profile_image_url (user)));

  mojito_set_add (source->priv->set, (GObject*)item);
  g_object_unref (item);
}

static void
timeline_received_cb (TwitterClient *client,
                      gpointer       user_data)
{
  MojitoSourceTwitter *source = MOJITO_SOURCE_TWITTER (user_data);

  source->priv->callback ((MojitoSource*)source, mojito_set_ref (source->priv->set), source->priv->user_data);
}

static void
update (MojitoSource *source, MojitoSourceDataFunc callback, gpointer user_data)
{
  MojitoSourceTwitter *twitter = (MojitoSourceTwitter*)source;
  MojitoSourceTwitterPrivate *priv = twitter->priv;

  if (!priv->user_set || !priv->password_set) {
    callback (source, NULL, user_data);
    return;
  }

  g_debug ("Updating Twitter...");

  /* TODO grim */
  twitter->priv->callback = callback;
  twitter->priv->user_data = user_data;
  mojito_set_empty (twitter->priv->set);

  twitter_client_get_friends_timeline (twitter->priv->client, NULL, 0);
}

static char *
mojito_source_twitter_get_name (MojitoSource *source)
{
  return "twitter";
}

static void
mojito_source_twitter_dispose (GObject *object)
{
  MojitoSourceTwitterPrivate *priv = MOJITO_SOURCE_TWITTER (object)->priv;

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

  G_OBJECT_CLASS (mojito_source_twitter_parent_class)->dispose (object);
}

static void
mojito_source_twitter_finalize (GObject *object)
{
  //MojitoSourceTwitterPrivate *priv = MOJITO_SOURCE_TWITTER (object)->priv;

  G_OBJECT_CLASS (mojito_source_twitter_parent_class)->finalize (object);
}

static void
mojito_source_twitter_class_init (MojitoSourceTwitterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoSourceClass *source_class = MOJITO_SOURCE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoSourceTwitterPrivate));

  object_class->dispose = mojito_source_twitter_dispose;
  object_class->finalize = mojito_source_twitter_finalize;

  source_class->get_name = mojito_source_twitter_get_name;
  source_class->update = update;
}

static void
mojito_source_twitter_init (MojitoSourceTwitter *self)
{
  MojitoSourceTwitterPrivate *priv;

  self->priv = priv = GET_PRIVATE (self);

  priv->user_set = priv->password_set = FALSE;

  /* TODO: when the image fetching is async change this to async */
  priv->soup = soup_session_sync_new ();

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
