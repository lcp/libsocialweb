#include <mojito/mojito-source.h>
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>

#include "lastfm.h"

G_DEFINE_TYPE (MojitoSourceLastfm, mojito_source_lastfm, MOJITO_TYPE_SOURCE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SOURCE_LASTFM, MojitoSourceLastfmPrivate))

struct _MojitoSourceLastfmPrivate {
  RestProxy *proxy;
};

#if 0
static void
rest_callback (RestProxyCall *call,
               GError *error,
               GObject *weak_object,
               gpointer userdata)
{
  MojitoSourceLastfm *source = MOJITO_SOURCE_LASTFM (weak_object);
  MojitoSourceLastfmPrivate *priv = source->priv;
  RestXmlParser *parser;
  RestXmlNode *root, *node;

  if (error) {
    g_printerr ("Cannot get Last.fm friends: %s\n", error->message);
    return;
  }

  parser = rest_xml_parser_new ();
  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));
  g_object_unref (call);

  /* TODO: check for failure in lfm root element */
  //node = rest_xml_node_find (root, "lfm");

  for (node = rest_xml_node_find (root, "user"); node; node = node->next) {
    GHashTable *hash;

    hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

    g_debug ("lastfm %s", rest_xml_node_find (node, "realname")->content);
  }

  rest_xml_node_free (root);
  g_object_unref (parser);
}

static gboolean
invoke (gpointer user_data)
{
  MojitoSourceLastfm *source = user_data;
  RestProxyCall *call;

  call = rest_proxy_new_call (source->priv->proxy);
  rest_proxy_call_add_params (call,
                              /* TODO: get proper API key */
                              "api_key", "aa581f6505fd3ea79073ddcc2215cbc7",
                              "method", "user.getFriends",
                              /* TODO: parameterize */
                              "user", "rossburton",
                              "recenttracks", "1",
                              "limit", "10",
                              NULL);
  /* TODO: GError */
  rest_proxy_call_async (call, rest_callback, (GObject*)source, NULL, NULL);

  return TRUE;
}
#endif

static void
update (MojitoSource *source, MojitoSourceDataFunc callback, gpointer user_data)
{
  callback (source, NULL, user_data);
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
