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
  guint timeout_id;
  RestProxy *proxy;
  char *user_id;
  /* Hash of UUID -> data hash */
  GHashTable *cache;
};

static GList *
mojito_source_flickr_initialize (MojitoSourceClass *source_class, MojitoCore *core)
{
  MojitoSourceFlickr *source;
  
  mojito_photos_initialize (core);
    
  /* TODO: replace with configuration file */
  source = g_object_new (MOJITO_TYPE_SOURCE_FLICKR, NULL);
  GET_PRIVATE (source)->core = core;
  
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
flickr_callback (RestProxyCall *call,
                 GError *error,
                 GObject *weak_object,
                 gpointer userdata)
{
  MojitoSourceFlickr *source = MOJITO_SOURCE_FLICKR (weak_object);
  MojitoSourceFlickrPrivate *priv = source->priv;
  RestXmlParser *parser;
  RestXmlNode *root, *node;

  if (error) {
    g_printerr ("Cannot get Flickr photos: %s", error->message);
    return;
  }

  parser = rest_xml_parser_new ();
  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));
  g_object_unref (call);
  
  node = rest_xml_node_find (root, "rsp");
  /* TODO: check for failure */

  node = rest_xml_node_find (root, "photos");
  for (node = rest_xml_node_find (node, "photo"); node; node = node->next) {
    GHashTable *hash;
    char *id;

    id = g_strdup (rest_xml_node_get_attr (node, "id"));
    /* TODO: check that the item doesn't exist in the cache */

    hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert (hash, "id", id);
    g_hash_table_insert (hash, "title", g_strdup (rest_xml_node_get_attr (node, "title")));
    /* TODO: decide on a format for standard date metadata */
    //date = atoi (rest_xml_node_get_attr (node, "dateupload"));
    g_hash_table_insert (hash, "url", construct_photo_page_url (node));

    /* Cache the data */
    g_hash_table_insert (priv->cache, g_strdup (id), hash);

    /* TODO: is this reffing required? */
    g_signal_emit_by_name (source, "item-added", "flickr", g_hash_table_ref (hash));
    g_hash_table_unref (hash);
  }

  rest_xml_node_free (root);
  g_object_unref (parser);
}

static gboolean
invoke_flickr (MojitoSourceFlickr *source)
{
  RestProxyCall *call;
  
  g_debug ("Updating Flickr");
  
  call = rest_proxy_new_call (source->priv->proxy);
  rest_proxy_call_add_params (call,
                              "method", "flickr.photos.getContactsPublicPhotos",
                              /* TODO: this is a temporary key */
                              "api_key", "cf4e02fc57240a9b07346ad26e291080",
                              "user_id", source->priv->user_id,
                              "extras", "date_upload",
                              NULL);
  /* TODO: GError */
  rest_proxy_call_async (call, flickr_callback, (GObject*)source, NULL, NULL);

  return TRUE;
}

static void
mojito_source_flickr_start (MojitoSource *source)
{
  MojitoSourceFlickr *flickr = MOJITO_SOURCE_FLICKR (source);
  MojitoSourceFlickrPrivate *priv = flickr->priv;

  /* Are we already running? */
  if (priv->timeout_id) {
    /* Yes, so emit cached items.  We rely on the views to filter out items they've
       already seen */
    GList *values = g_hash_table_get_keys (priv->cache);
    while (values) {
      g_signal_emit_by_name (source, "item-added", "flickr", values->data);
      values = g_list_delete_link (values, values);
    }
  } else {
    /* No.  Do the initial call and schedule future runs */
    invoke_flickr (MOJITO_SOURCE_FLICKR (source));
    priv->timeout_id = g_timeout_add_seconds (10*60, (GSourceFunc)invoke_flickr, source);
  }
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
  source_class->start = mojito_source_flickr_start;
}

static void
mojito_source_flickr_init (MojitoSourceFlickr *self)
{
  self->priv = GET_PRIVATE (self);
  
  self->priv->proxy = rest_proxy_new ("http://api.flickr.com/services/rest/", FALSE);
  /* TODO: get from configuration file */
  self->priv->user_id = g_strdup ("35468147630@N01");
  self->priv->cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, (GDestroyNotify)g_hash_table_unref);
}

MojitoSource *
mojito_source_flickr_new (MojitoCore *core)
{
  MojitoSourceFlickr *source = g_object_new (MOJITO_TYPE_SOURCE_FLICKR, NULL);
  GET_PRIVATE (source)->core = core;
  return MOJITO_SOURCE (source);
}
