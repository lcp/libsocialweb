#include "mojito-source-flickr.h"
#include "mojito-web.h"
#include "mojito-utils.h"
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include "generic.h"

G_DEFINE_TYPE (MojitoSourceFlickr, mojito_source_flickr, MOJITO_TYPE_SOURCE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SOURCE_FLICKR, MojitoSourceFlickrPrivate))

struct _MojitoSourceFlickrPrivate {
  MojitoCore *core; /* TODO: move to MojitoSource */
  RestProxy *proxy;
};

static GList *
mojito_source_flickr_initialize (MojitoSourceClass *source_class, MojitoCore *core)
{
  MojitoSourceFlickr *source;
  
  /* TODO: replace with configuration file */
  source = g_object_new (MOJITO_TYPE_SOURCE_FLICKR, NULL);
  GET_PRIVATE (source)->core = core;
  
  return g_list_prepend (NULL, source);
}

static void
mojito_source_flickr_update (MojitoSource *source)
{
  MojitoSourceFlickrPrivate *priv = MOJITO_SOURCE_FLICKR (source)->priv;
  char *payload;
  goffset len;
  GError *error = NULL;
  
  g_debug ("Updating Flickr");
  
  if (!rest_proxy_simple_run (priv->proxy, &payload, &len, &error,
                             "method", "flickr.photos.getContactsPublicPhotos",
                             /* TODO: this is a temporary key */
                             "api_key", "cf4e02fc57240a9b07346ad26e291080",
                              /* TODO: get from configuration file */
                             "user_id", "35468147630@N01",
                             "extras", "date_upload,owner_name",
                             NULL)) {
    g_printerr ("Cannot fetch Flickr photos: %s", error->message);
    g_error_free (error);
    return;
  }

  RestXmlParser *parser = rest_xml_parser_new ();
  RestXmlNode *root = rest_xml_parser_parse_from_data (parser, payload, len);
  RestXmlNode *node = rest_xml_node_find (root, "rsp");
  /* TODO: check for failure */
  
  node = rest_xml_node_find (root, "photos");
  for (node = rest_xml_node_find (node, "photo"); node; node = node->next) {
    g_debug ("title %s", rest_xml_node_get_attr (node, "title"));
  }

  rest_xml_node_free (root);
  g_object_unref (parser);
}

static void
mojito_source_flickr_dispose (GObject *object)
{
  MojitoSourceFlickrPrivate *priv = MOJITO_SOURCE_FLICKR (object)->priv;
  
  /* ->core is a weak reference so we don't unref it here */
  
  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }
  
  G_OBJECT_CLASS (mojito_source_flickr_parent_class)->dispose (object);
}

static void
mojito_source_flickr_class_init (MojitoSourceFlickrClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoSourceClass *source_class = MOJITO_SOURCE_CLASS (klass);
  
  g_type_class_add_private (klass, sizeof (MojitoSourceFlickrPrivate));
  
  object_class->dispose = mojito_source_flickr_dispose;
  
  source_class->initialize = mojito_source_flickr_initialize;  
  source_class->update = mojito_source_flickr_update;
}

static void
mojito_source_flickr_init (MojitoSourceFlickr *self)
{
  self->priv = GET_PRIVATE (self);
  
  self->priv->proxy = rest_proxy_new ("http://api.flickr.com/services/rest/", FALSE);
}

MojitoSource *
mojito_source_flickr_new (MojitoCore *core)
{
  MojitoSourceFlickr *source = g_object_new (MOJITO_TYPE_SOURCE_FLICKR, NULL);
  GET_PRIVATE (source)->core = core;
  return MOJITO_SOURCE (source);
}
