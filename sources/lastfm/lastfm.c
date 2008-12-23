#include <stdlib.h>
#include <mojito/mojito-source.h>
#include <mojito/mojito-item.h>
#include <mojito/mojito-utils.h>
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>

#include "lastfm.h"

G_DEFINE_TYPE (MojitoSourceLastfm, mojito_source_lastfm, MOJITO_TYPE_SOURCE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SOURCE_LASTFM, MojitoSourceLastfmPrivate))

struct _MojitoSourceLastfmPrivate {
  RestProxy *proxy;
};

static void
update (MojitoSource *source, MojitoSourceDataFunc callback, gpointer user_data)
{
  MojitoSourceLastfm *lastfm = MOJITO_SOURCE_LASTFM (source);
  RestProxyCall *call;
  GError *error = NULL;
  RestXmlParser *parser;
  RestXmlNode *root, *node;
  MojitoSet *set;

  g_debug ("Updating last.fm");

  call = rest_proxy_new_call (lastfm->priv->proxy);
  rest_proxy_call_add_params (call,
                              /* TODO: get proper API key */
                              "api_key", "aa581f6505fd3ea79073ddcc2215cbc7",
                              "method", "user.getFriends",
                              /* TODO: parameterize */
                              "user", "rossburton",
                              NULL);
  /* TODO: GError */
  if (!rest_proxy_call_run (call, NULL, &error)) {
    g_printerr ("Cannot get Last.fm friends: %s\n", error->message);
    g_error_free (error);
    g_object_unref (call);

    callback (source, NULL, user_data);
    return;
  }

  parser = rest_xml_parser_new ();
  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));
  g_object_unref (call);
  g_object_unref (parser);

  set = mojito_item_set_new ();

  /* TODO: check for failure in lfm root element */
  for (node = rest_xml_node_find (root, "user"); node; node = node->next) {
    MojitoItem *item;
    RestXmlNode *recent, *track, *date;
    const char *s;

    call = rest_proxy_new_call (lastfm->priv->proxy);
    rest_proxy_call_add_params (call,
                                /* TODO: get proper API key */
                                "api_key", "aa581f6505fd3ea79073ddcc2215cbc7",
                                "method", "user.getRecentTracks",
                                "user", rest_xml_node_find (node, "name")->content,
                                "limit", "1",
                                NULL);
    /* TODO: GError */
    if (!rest_proxy_call_run (call, NULL, &error)) {
      g_printerr ("Cannot get Last.fm recent tracks: %s\n", error->message);
      g_error_free (error);
      g_object_unref (call);
      /* TODO: proper cleanup */
      callback (source, NULL, user_data);
      return;
    }

    parser = rest_xml_parser_new ();
    recent = rest_xml_parser_parse_from_data (parser,
                                                   rest_proxy_call_get_payload (call),
                                                   rest_proxy_call_get_payload_length (call));
    g_object_unref (call);
    g_object_unref (parser);

    track = rest_xml_node_find (recent, "track");

    if (!track) {
      rest_xml_node_unref (recent);
      continue;
    }

    item = mojito_item_new ();
    mojito_item_set_source (item, source);

    /* TODO user+track url? user+timestamp? */
    mojito_item_put (item, "id", rest_xml_node_find (track, "url")->content);
    mojito_item_put (item, "link", rest_xml_node_find (track, "url")->content);
    /* TODO: track by artist from album? */
    mojito_item_put (item, "title", rest_xml_node_find (track, "name")->content);

    date = rest_xml_node_find (track, "date");
    mojito_item_take (item, "date", mojito_time_t_to_string (atoi (rest_xml_node_get_attr (date, "uts"))));

    s = rest_xml_node_find (node, "realname")->content;
    if (s) mojito_item_put (item, "author", s);
    mojito_item_put (item, "authorid", rest_xml_node_find (node, "name")->content);
    /* TODO: buddyicon from medium image */

    rest_xml_node_unref (recent);

    mojito_set_add (set, (GObject*)item);
  }

  rest_xml_node_unref (root);

  callback (source, set, user_data);
}

static char *
get_name (MojitoSource *source)
{
  return "lastfm";
}

static void
mojito_source_lastfm_dispose (GObject *object)
{
  MojitoSourceLastfmPrivate *priv = ((MojitoSourceLastfm*)object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (mojito_source_lastfm_parent_class)->dispose (object);
}

static void
mojito_source_lastfm_finalize (GObject *object)
{
  G_OBJECT_CLASS (mojito_source_lastfm_parent_class)->finalize (object);
}

static void
mojito_source_lastfm_class_init (MojitoSourceLastfmClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoSourceClass *source_class = MOJITO_SOURCE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoSourceLastfmPrivate));

  object_class->dispose = mojito_source_lastfm_dispose;
  object_class->finalize = mojito_source_lastfm_finalize;

  source_class->get_name = get_name;
  source_class->update = update;
}

static void
mojito_source_lastfm_init (MojitoSourceLastfm *self)
{
  self->priv = GET_PRIVATE (self);

  self->priv->proxy = rest_proxy_new ("http://ws.audioscrobbler.com/2.0/", FALSE);
}
