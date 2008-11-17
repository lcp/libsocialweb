#include <stdlib.h>
#include "mojito-source-flickr.h"
#include "mojito-photos.h"
#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>
#include "generic.h"

G_DEFINE_TYPE (MojitoSourceFlickr, mojito_source_flickr, MOJITO_TYPE_SOURCE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SOURCE_FLICKR, MojitoSourceFlickrPrivate))

struct _MojitoSourceFlickrPrivate {
  MojitoCore *core; /* TODO: move to MojitoSource */
  RestProxy *proxy;
  char *user_id;
};

static GList *
mojito_source_flickr_initialize (MojitoSourceClass *source_class, MojitoCore *core)
{
  MojitoSourceFlickr *source;
  
  mojito_photos_initialize (core);
    
  /* TODO: replace with configuration file */
  source = g_object_new (MOJITO_TYPE_SOURCE_FLICKR, NULL);
  GET_PRIVATE (source)->core = core;
  /* TODO: get from configuration file */
  GET_PRIVATE (source)->user_id = g_strdup ("35468147630@N01");
  
  return g_list_prepend (NULL, source);
}

static char *
construct_photo_page_url (RestXmlNode *node)
{
  g_assert (node != NULL);
  
  return g_strdup_printf ("http://www.flickr.com/photos/%s/%s/",
                          rest_xml_node_get_attr (node, "owner"),
                          rest_xml_node_get_attr (node, "id"));
}

static void
mojito_source_flickr_update (MojitoSource *source)
{
  MojitoSourceFlickrPrivate *priv = MOJITO_SOURCE_FLICKR (source)->priv;
  char *payload;
  goffset len;
  GError *error = NULL;
  RestXmlParser *parser;
  RestXmlNode *root, *node;
  
  g_debug ("Updating Flickr");
  
  if (!rest_proxy_simple_run (priv->proxy, &payload, &len, &error,
                              "method", "flickr.photos.getContactsPublicPhotos",
                              /* TODO: this is a temporary key */
                              "api_key", "cf4e02fc57240a9b07346ad26e291080",
                              "user_id", priv->user_id,
                              "extras", "date_upload",
                              NULL)) {
    g_printerr ("Cannot fetch Flickr photos: %s", error->message);
    g_error_free (error);
    return;
  }

  parser = rest_xml_parser_new ();
  root = rest_xml_parser_parse_from_data (parser, payload, len);
  g_free (payload);
  
  node = rest_xml_node_find (root, "rsp");
  /* TODO: check for failure */

  /* TODO: If we ever support multiple Flickr sources, this should be changed */
  mojito_photos_remove (priv->core, "http://flickr.com/");

  node = rest_xml_node_find (root, "photos");
  for (node = rest_xml_node_find (node, "photo"); node; node = node->next) {
    const char *id, *title;
    char *url;
    time_t date;

    id = rest_xml_node_get_attr (node, "id");
    title = rest_xml_node_get_attr (node, "title");
    date = atoi (rest_xml_node_get_attr (node, "dateupload"));
    url = construct_photo_page_url (node);
    
    mojito_photos_add (priv->core, "http://flickr.com/",
                       id, date, url, title);
    
    g_free (url);
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
mojito_source_flickr_finalize (GObject *object)
{
  MojitoSourceFlickrPrivate *priv = MOJITO_SOURCE_FLICKR (object)->priv;
  
  g_free (priv->user_id);
  
  G_OBJECT_CLASS (mojito_source_flickr_parent_class)->finalize (object);
}

static void
mojito_source_flickr_class_init (MojitoSourceFlickrClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoSourceClass *source_class = MOJITO_SOURCE_CLASS (klass);
  
  g_type_class_add_private (klass, sizeof (MojitoSourceFlickrPrivate));
  
  object_class->dispose = mojito_source_flickr_dispose;
  object_class->finalize = mojito_source_flickr_finalize;
  
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
