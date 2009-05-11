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
#include <time.h>
#include <string.h>
#include "myspace.h"
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>
#include <mojito/mojito-utils.h>
#include <mojito/mojito-web.h>
#include <mojito-keyfob/mojito-keyfob.h>
#include <rest/oauth-proxy.h>
#include <rest/rest-xml-parser.h>

G_DEFINE_TYPE (MojitoServiceMySpace, mojito_service_myspace, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_MYSPACE, MojitoServiceMySpacePrivate))

struct _MojitoServiceMySpacePrivate {
  RestProxy *proxy;
  char *user_id;
  char *display_name;
  char *profile_url;
  char *image_url;
};

RestXmlNode *
node_from_call (RestProxyCall *call)
{
  static RestXmlParser *parser = NULL;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  /* TODO: check call return status? */

  return rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));
}

static char *
get_utc_date (const char *s)
{
  struct tm tm;
  time_t t;

  if (s == NULL)
    return NULL;

  strptime (s, "%d/%m/%Y %T", &tm);
  t = mktime (&tm);
  /* TODO: This is a very bad timezone correction */
  t += (8 * 60 * 60); /* 8 hours */

  return mojito_time_t_to_string (t);
}

static void
got_status_cb (RestProxyCall *call,
             GError        *error,
             GObject       *weak_object,
             gpointer       userdata)
{
  MojitoService *service = MOJITO_SERVICE (weak_object);
  MojitoServiceMySpacePrivate *priv = MOJITO_SERVICE_MYSPACE (service)->priv;
  RestXmlNode *root, *node;
  MojitoSet *set;

  root = node_from_call (call);

  set = mojito_item_set_new ();

  /*
   * The result of /status is a <user> node, whereas /friends/status is
   * <user><friends><user>. Look closely to find out what data we have
   */
  node = rest_xml_node_find (root, "friends");
  if (node) {
    node = rest_xml_node_find (node, "user");
  } else {
    node = root;
  }

  while (node) {
    /*
      <user>
      <userid>188488921</userid>
      <imageurl>http://c3.ac-images.myspacecdn.com/images02/110/s_768909a648e740939422bdc875ff2bf2.jpg</imageurl>
      <profileurl>http://www.myspace.com/cwiiis</profileurl>
      <name>Cwiiis</name>
      <mood>neutral</mood>
      <moodimageurl>http://x.myspacecdn.com/images/blog/moods/iBrads/amused.gif</moodimageurl>
      <moodlastupdated>15/04/2009 04:20:59</moodlastupdated>
      <status>haha, Ross has myspace</status>
      </user>
    */
    MojitoItem *item;
    char *id;
    RestXmlNode *subnode;

    item = mojito_item_new ();
    mojito_item_set_service (item, service);

    id = g_strconcat ("myspace-",
                      rest_xml_node_find (node, "userid")->content,
                      "-",
                      rest_xml_node_find (node, "moodlastupdated")->content,
                      NULL);
    mojito_item_take (item, "id", id);

    mojito_item_take (item, "date",
                      get_utc_date (rest_xml_node_find (node, "moodlastupdated")->content));

    mojito_item_put (item, "authorid", rest_xml_node_find (node, "userid")->content);
    subnode = rest_xml_node_find (node, "name");
    if (subnode && subnode->content)
      mojito_item_put (item, "author", subnode->content);
    else
      mojito_item_put (item, "author", priv->display_name);

    /* TODO: async downloading */
    subnode = rest_xml_node_find (node, "imageurl");
    if (subnode && subnode->content)
      mojito_item_take (item, "authoricon", mojito_web_download_image (subnode->content));

    mojito_item_put (item, "content", rest_xml_node_find (node, "status")->content);
    /* TODO: if mood is not "(none)" then append that to the status message */

    subnode = rest_xml_node_find (node, "profileurl");
    if (subnode && subnode->content)
      mojito_item_put (item, "url", subnode->content);
    else
      mojito_item_put (item, "url", priv->profile_url);

    mojito_set_add (set, G_OBJECT (item));
    g_object_unref (item);

    node = node->next;
  }

  if (!mojito_set_is_empty (set))
    mojito_service_emit_refreshed (service, set);

  mojito_set_unref (set);
}

static void
get_status_updates (MojitoServiceMySpace *service)
{
  MojitoServiceMySpacePrivate *priv = service->priv;
  char *function;
  RestProxyCall *call;
  GHashTable *params = NULL;

  g_assert (priv->user_id);

  call = rest_proxy_new_call (priv->proxy);

  g_object_get (service, "params", &params, NULL);

  if (params && g_hash_table_lookup (params, "own")) {
    function = g_strdup_printf ("v1/users/%s/status", priv->user_id);
    rest_proxy_call_set_function (call, function);
    g_free (function);
  } else {
    function = g_strdup_printf ("v1/users/%s/friends/status", priv->user_id);
    rest_proxy_call_set_function (call, function);
    g_free (function);
  }

  if (params)
    g_hash_table_unref (params);

  rest_proxy_call_async (call, got_status_cb, (GObject*)service, NULL, NULL);
}

static void
got_user_cb (RestProxyCall *call,
             GError        *error,
             GObject       *weak_object,
             gpointer       userdata)
{
  MojitoServiceMySpace *service = MOJITO_SERVICE_MYSPACE (weak_object);
  RestXmlNode *node;

  node = node_from_call (call);

  service->priv->user_id = rest_xml_node_find (node, "userid")->content;
  service->priv->display_name = rest_xml_node_find (node, "displayname")->content;
  service->priv->profile_url = rest_xml_node_find (node, "weburi")->content;
  service->priv->image_url = rest_xml_node_find (node, "imageuri")->content;

  get_status_updates (service);
}

static void
got_tokens_cb (OAuthProxy *proxy, gboolean authorised, gpointer user_data)
{
  MojitoServiceMySpace *myspace = user_data;
  MojitoServiceMySpacePrivate *priv = myspace->priv;
  RestProxyCall *call;

  if (authorised) {
    call = rest_proxy_new_call (priv->proxy);
    rest_proxy_call_set_function (call, "v1/user");
    rest_proxy_call_async (call, got_user_cb, (GObject*)myspace, NULL, NULL);
  }
}

static void
refresh (MojitoService *service)
{
  MojitoServiceMySpace *myspace = (MojitoServiceMySpace*)service;
  MojitoServiceMySpacePrivate *priv = myspace->priv;

  if (priv->user_id == NULL) {
    mojito_keyfob_oauth ((OAuthProxy*)priv->proxy, got_tokens_cb, service);
  } else {
    get_status_updates (myspace);
  }
}

static gboolean
sync_auth (MojitoServiceMySpace *myspace)
{
  MojitoServiceMySpacePrivate *priv = myspace->priv;

  if (priv->user_id == NULL) {
    RestProxyCall *call;
    RestXmlNode *node;

    if (!mojito_keyfob_oauth_sync ((OAuthProxy*)priv->proxy))
      return FALSE;

    call = rest_proxy_new_call (priv->proxy);
    rest_proxy_call_set_function (call, "v1/user");
    if (!rest_proxy_call_run (call, NULL, NULL))
      return FALSE;

    node = node_from_call (call);
    priv->user_id = rest_xml_node_find (node, "userid")->content;
    priv->display_name = rest_xml_node_find (node, "displayname")->content;
    priv->profile_url = rest_xml_node_find (node, "weburi")->content;
    priv->image_url = rest_xml_node_find (node, "imageuri")->content;
  }

  return TRUE;
}

static guint32
get_capabilities (MojitoService *service)
{
  MojitoServiceMySpace *myspace = MOJITO_SERVICE_MYSPACE (service);

  if (sync_auth (myspace))
    return SERVICE_CAN_UPDATE_STATUS | SERVICE_CAN_GET_PERSONA_ICON;
  else
    return 0;
}

static gchar *
get_persona_icon (MojitoService *service)
{
  MojitoServiceMySpace *myspace = MOJITO_SERVICE_MYSPACE (service);
  MojitoServiceMySpacePrivate *priv = myspace->priv;

  if (sync_auth (myspace))
    return mojito_web_download_image (priv->image_url);
  else
    return NULL;
}

static gboolean
update_status (MojitoService *service, const char *msg)
{
  MojitoServiceMySpace *myspace = MOJITO_SERVICE_MYSPACE (service);
  MojitoServiceMySpacePrivate *priv = myspace->priv;
  RestProxyCall *call;
  char *function;
  gboolean ret;

  if (!sync_auth (myspace))
    return FALSE;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_method (call, "PUT");
  function = g_strdup_printf ("v1/users/%s/status", priv->user_id);
  rest_proxy_call_set_function (call, function);
  g_free (function);

  rest_proxy_call_add_params (call,
                              "userId", priv->user_id,
                              "status", msg,
                              NULL);

  ret = rest_proxy_call_run (call, NULL, NULL);

  g_object_unref (call);

  return ret;
}

static const char *
mojito_service_myspace_get_name (MojitoService *service)
{
  return "myspace";
}

static void
mojito_service_myspace_dispose (GObject *object)
{
  MojitoServiceMySpacePrivate *priv = MOJITO_SERVICE_MYSPACE (object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (mojito_service_myspace_parent_class)->dispose (object);
}

static void
mojito_service_myspace_finalize (GObject *object)
{
  MojitoServiceMySpacePrivate *priv = MOJITO_SERVICE_MYSPACE (object)->priv;

  g_free (priv->user_id);

  G_OBJECT_CLASS (mojito_service_myspace_parent_class)->finalize (object);
}

static void
mojito_service_myspace_class_init (MojitoServiceMySpaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoServiceClass *service_class = MOJITO_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoServiceMySpacePrivate));

  object_class->dispose = mojito_service_myspace_dispose;
  object_class->finalize = mojito_service_myspace_finalize;

  service_class->get_name = mojito_service_myspace_get_name;
  service_class->get_capabilities = get_capabilities;
  service_class->get_persona_icon = get_persona_icon;
  service_class->update_status = update_status;
  service_class->refresh = refresh;
}

static void
mojito_service_myspace_init (MojitoServiceMySpace *self)
{
  MojitoServiceMySpacePrivate *priv;

  priv = self->priv = GET_PRIVATE (self);

  priv->proxy = oauth_proxy_new (MYSPACE_APIKEY, MYSPACE_SECRET,
                                 "http://api.myspace.com/", FALSE);
}
