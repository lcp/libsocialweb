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
#include <mojito-keystore/mojito-keystore.h>
#include <rest/oauth-proxy.h>
#include <rest/rest-xml-parser.h>
#include <mojito/mojito-online.h>

G_DEFINE_TYPE (MojitoServiceMySpace, mojito_service_myspace, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_MYSPACE, MojitoServiceMySpacePrivate))

struct _MojitoServiceMySpacePrivate {
  gboolean running;
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
  RestXmlNode *root;

  if (call == NULL)
    return NULL;

  if (parser == NULL)
    parser = rest_xml_parser_new ();

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_message ("Error from MySpace: %s (%d)",
               rest_proxy_call_get_status_message (call),
               rest_proxy_call_get_status_code (call));
    return NULL;
  }

  root = rest_xml_parser_parse_from_data (parser,
                                          rest_proxy_call_get_payload (call),
                                          rest_proxy_call_get_payload_length (call));

  if (root == NULL) {
    g_message ("Invalid XML from MySpace: %s",
               rest_proxy_call_get_payload (call));
    return NULL;
  }

  if (strcmp (root->name, "error") != 0) {
    return root;
  } else {
    RestXmlNode *node;
    node = rest_xml_node_find (root, "statusdescription");
    if (node) {
      g_message ("Error: %s", node->content);
    } else {
      g_message ("Error from MySpace: %s",
                 rest_proxy_call_get_payload (call));
    }
    rest_xml_node_unref (root);
    return NULL;
  }
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
               const GError  *error,
               GObject       *weak_object,
               gpointer       userdata)
{
  MojitoService *service = MOJITO_SERVICE (weak_object);
  MojitoServiceMySpacePrivate *priv = MOJITO_SERVICE_MYSPACE (service)->priv;
  RestXmlNode *root, *node;
  MojitoSet *set;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  root = node_from_call (call);
  if (!root)
    return;

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
      mojito_item_request_image_fetch (item, "authoricon", subnode->content);

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

  rest_xml_node_unref (root);

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

/*
 * For a given parent @node, get the child node called @name and return a copy
 * of the content, or NULL. If the content is the empty string, NULL is
 * returned.
 */
static char *
get_child_node_value (RestXmlNode *node, const char *name)
{
  RestXmlNode *subnode;

  g_assert (node);
  g_assert (name);

  subnode = rest_xml_node_find (node, name);
  if (!subnode)
    return NULL;

  if (subnode->content && subnode->content[0])
    return g_strdup (subnode->content);
  else
    return NULL;
}

static const char **
get_static_caps (MojitoService *service)
{
  static const char * caps[] = {
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };

  return caps;
}

static const char **
get_dynamic_caps (MojitoService *service)
{
  MojitoServiceMySpace *myspace = MOJITO_SERVICE_MYSPACE (service);
  static const char * caps[] = {
    CAN_UPDATE_STATUS,
    CAN_REQUEST_AVATAR,
    NULL
  };
  static const char * no_caps[] = { NULL };

  if (myspace->priv->user_id)
    return caps;
  else
    return no_caps;
}

static void
got_user_cb (RestProxyCall *call,
             const GError  *error,
             GObject       *weak_object,
             gpointer       userdata)
{
  MojitoService *service = MOJITO_SERVICE (weak_object);
  MojitoServiceMySpace *myspace = MOJITO_SERVICE_MYSPACE (service);
  MojitoServiceMySpacePrivate *priv = myspace->priv;
  RestXmlNode *node;

  if (error) {
    g_message ("Error: %s", error->message);
    return;
  }

  node = node_from_call (call);
  if (!node)
    return;

  priv->user_id = get_child_node_value (node, "userid");
  priv->display_name = get_child_node_value (node, "displayname");
  priv->profile_url = get_child_node_value (node, "weburi");
  priv->image_url = get_child_node_value (node, "imageuri");

  rest_xml_node_unref (node);

  mojito_service_emit_capabilities_changed
    (service, get_dynamic_caps (service));

  if (priv->running)
    get_status_updates (myspace);
}

static void
got_tokens_cb (RestProxy *proxy, gboolean authorised, gpointer user_data)
{
  MojitoServiceMySpace *myspace = MOJITO_SERVICE_MYSPACE (user_data);
  MojitoServiceMySpacePrivate *priv = myspace->priv;
  RestProxyCall *call;

  if (authorised) {
    call = rest_proxy_new_call (priv->proxy);
    rest_proxy_call_set_function (call, "v1/user");
    rest_proxy_call_async (call, got_user_cb, (GObject*)myspace, NULL, NULL);
  } else {
    mojito_service_emit_refreshed ((MojitoService *)myspace, NULL);
  }
}

static void
start (MojitoService *service)
{
  MojitoServiceMySpace *myspace = (MojitoServiceMySpace*)service;

  myspace->priv->running = TRUE;
}

static void
refresh (MojitoService *service)
{
  MojitoServiceMySpace *myspace = (MojitoServiceMySpace*)service;
  MojitoServiceMySpacePrivate *priv = myspace->priv;

  if (!priv->running)
    return;

  if (priv->user_id == NULL) {
    mojito_keyfob_oauth ((OAuthProxy*)priv->proxy, got_tokens_cb, service);
  } else {
    get_status_updates (myspace);
  }
}

static void
_avatar_downloaded_cb (const gchar *uri,
                       gchar       *local_path,
                       gpointer     userdata)
{
  MojitoService *service = MOJITO_SERVICE (userdata);

  mojito_service_emit_avatar_retrieved (service, local_path);
  g_free (local_path);
}

static void
request_avatar (MojitoService *service)
{
  MojitoServiceMySpacePrivate *priv = GET_PRIVATE (service);

  if (priv->image_url)
  {
    mojito_web_download_image_async (priv->image_url,
                                     _avatar_downloaded_cb,
                                     service);
  } else {
    mojito_service_emit_avatar_retrieved (service, NULL);
  }
}

static void
_status_updated_cb (RestProxyCall *call,
                    const GError  *error,
                    GObject       *weak_object,
                    gpointer       userdata)
{
  MojitoService *service = MOJITO_SERVICE (weak_object);

  mojito_service_emit_status_updated (service, error == NULL);
}

static void
update_status (MojitoService *service, const char *msg)
{
  MojitoServiceMySpace *myspace = MOJITO_SERVICE_MYSPACE (service);
  MojitoServiceMySpacePrivate *priv = myspace->priv;
  RestProxyCall *call;
  char *function;

  if (!priv->proxy)
    return;

  call = rest_proxy_new_call (priv->proxy);
  rest_proxy_call_set_method (call, "PUT");
  function = g_strdup_printf ("v1/users/%s/status", priv->user_id);
  rest_proxy_call_set_function (call, function);
  g_free (function);

  rest_proxy_call_add_params (call,
                              "userId", priv->user_id,
                              "status", msg,
                              NULL);

  rest_proxy_call_async (call, _status_updated_cb, (GObject *)service, NULL, NULL);
}

static const char *
mojito_service_myspace_get_name (MojitoService *service)
{
  return "myspace";
}

static void
online_notify (gboolean online, gpointer user_data)
{
  MojitoServiceMySpace *service = (MojitoServiceMySpace *) user_data;
  MojitoServiceMySpacePrivate *priv = service->priv;

  if (online) {
    const char *key = NULL, *secret = NULL;
    mojito_keystore_get_key_secret ("myspace", &key, &secret);
    priv->proxy = oauth_proxy_new (key, secret, "http://api.myspace.com/", FALSE);
    mojito_keyfob_oauth ((OAuthProxy *)priv->proxy, got_tokens_cb, service);
  } else {
    mojito_service_emit_capabilities_changed ((MojitoService *)service, NULL);

    if (priv->proxy) {
      g_object_unref (priv->proxy);
      priv->proxy = NULL;
    }

    g_free (priv->user_id);
    priv->user_id = NULL;
  }
}

static void
mojito_service_myspace_dispose (GObject *object)
{
  MojitoServiceMySpacePrivate *priv = MOJITO_SERVICE_MYSPACE (object)->priv;

  mojito_online_remove_notify (online_notify, object);

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
  service_class->get_static_caps = get_static_caps;
  service_class->get_dynamic_caps = get_dynamic_caps;
  service_class->update_status = update_status;
  service_class->start = start;
  service_class->refresh = refresh;
  service_class->request_avatar = request_avatar;
}

static void
mojito_service_myspace_init (MojitoServiceMySpace *self)
{
  self->priv = GET_PRIVATE (self);

  if (mojito_is_online ()) {
    online_notify (TRUE, self);
  }

  mojito_online_add_notify (online_notify, self);
}
