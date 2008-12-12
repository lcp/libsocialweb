#include <stdlib.h>
#include "twitter.h"
#include <twitter-glib/twitter-glib.h>
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>
#include <mojito/mojito-utils.h>

G_DEFINE_TYPE (MojitoSourceTwitter, mojito_source_twitter, MOJITO_TYPE_SOURCE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SOURCE_TWITTER, MojitoSourceTwitterPrivate))

struct _MojitoSourceTwitterPrivate {
  TwitterClient *client;
  TwitterAuthState auth_state;

  /* This is grim */
  MojitoSet *set;
  MojitoSourceDataFunc callback;
  gpointer user_data;
};

static gboolean
authenticate_cb (TwitterClient *client, TwitterAuthState state, gpointer user_data)
{
  MojitoSourceTwitter *source = user_data;
  source->priv->auth_state = state;
}

static void
status_received_cb (TwitterClient *client,
                    TwitterStatus *status,
                    const GError  *error,
                    gpointer       user_data)
{
  MojitoSourceTwitter *source = MOJITO_SOURCE_TWITTER (user_data);
  MojitoItem *item;
  char *url, *date;
  GTimeVal timeval;

  item = mojito_item_new ();
  mojito_item_set_source (item, MOJITO_SOURCE (source));

  url = g_strdup_printf ("http://twitter.com/%s/statuses/%d",
                         twitter_user_get_screen_name (twitter_status_get_user (status)),
                         twitter_status_get_id (status));

  twitter_date_to_time_val (twitter_status_get_created_at (status), &timeval);
  date = mojito_time_t_to_string (timeval.tv_sec);

  mojito_item_put (item, "id", url);
  mojito_item_put (item, "link", url);
  mojito_item_take (item, "date", date);
  /* TODO: need a better name for this */
  mojito_item_put (item, "content", twitter_status_get_text (status));

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

  twitter->priv->callback = callback;
  twitter->priv->user_data = user_data;
  mojito_set_empty (twitter->priv->set);

  switch (twitter->priv->auth_state) {
  case TWITTER_AUTH_SUCCESS:
  case TWITTER_AUTH_RETRY:
    twitter_client_get_friends_timeline (twitter->priv->client, NULL, 0);
    break;
  case TWITTER_AUTH_NEGOTIATING:
    /* Still authenticating, so return an empty set */
  case TWITTER_AUTH_FAILED:
    /* Authentication failed, so return an empty set */
    callback (source, mojito_set_new (), user_data);
    break;
  }
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
  self->priv = GET_PRIVATE (self);

  /* TODO: get from configuration file */
  self->priv->client = twitter_client_new_for_user ("ross@linux.intel.com", "password");
  g_signal_connect (self->priv->client, "authenticate",
                    G_CALLBACK (authenticate_cb),
                    self);
  g_signal_connect (self->priv->client, "status-received",
                    G_CALLBACK (status_received_cb),
                    self);
  g_signal_connect (self->priv->client, "timeline-complete",
                    G_CALLBACK (timeline_received_cb),
                    self);
  self->priv->auth_state = TWITTER_AUTH_RETRY;

  self->priv->set = mojito_item_set_new ();
  /* TODO: set user agent */
}
