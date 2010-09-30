/*
 * libsocialweb - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 *
 * Author: Rob Bradford <rob@linux.intel.com>
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libsocialweb/sw-utils.h>

#include <rest/rest-proxy.h>
#include <rest/rest-xml-parser.h>

#include <json-glib/json-glib.h>

#include "twitter-item-stream.h"

G_DEFINE_TYPE (SwTwitterItemStream, sw_twitter_item_stream, SW_TYPE_ITEM_STREAM)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_TWITTER_ITEM_STREAM, SwTwitterItemStreamPrivate))

typedef struct _SwTwitterItemStreamPrivate SwTwitterItemStreamPrivate;

struct _SwTwitterItemStreamPrivate {
  RestProxy *proxy;
  GHashTable *params;
  gchar *query;
  GString *cur_buffer;
  gint buf_size;
  JsonParser *parser;
};

enum
{
  PROP_0,
  PROP_PROXY,
  PROP_PARAMS,
  PROP_QUERY
};

static void
sw_twitter_item_stream_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  SwTwitterItemStreamPrivate *priv = GET_PRIVATE (object);
  switch (property_id) {
    case PROP_PROXY:
      g_value_set_object (value, priv->proxy);
      break;
    case PROP_PARAMS:
      g_value_set_boxed (value, priv->params);
      break;
    case PROP_QUERY:
      g_value_set_string (value, priv->query);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_twitter_item_stream_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  SwTwitterItemStreamPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_PROXY:
      if (priv->proxy)
      {
        g_object_unref (priv->proxy);
      }
      priv->proxy = g_value_dup_object (value);
      break;
    case PROP_PARAMS:
      priv->params = g_value_dup_boxed (value);
      break;
    case PROP_QUERY:
      priv->query = g_value_dup_string (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sw_twitter_item_stream_dispose (GObject *object)
{
  SwTwitterItemStreamPrivate *priv = GET_PRIVATE (object);

  if (priv->proxy)
  {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

/*
  g_signal_handlers_disconnect_by_func (sw_item_stream_get_service (item_stream),
                                        _service_item_hidden_cb,
                                        item_stream);
  g_signal_handlers_disconnect_by_func (sw_item_stream_get_service (item_stream),
                                        _service_user_changed_cb,
                                        item_stream);

  g_signal_handlers_disconnect_by_func (sw_item_stream_get_service (item_stream),
                                        _service_capabilities_changed_cb,
                                        item_stream);
*/
  G_OBJECT_CLASS (sw_twitter_item_stream_parent_class)->dispose (object);
}

static void
sw_twitter_item_stream_finalize (GObject *object)
{
  SwTwitterItemStreamPrivate *priv = GET_PRIVATE (object);

  g_free (priv->query);
  g_hash_table_unref (priv->params);

  G_OBJECT_CLASS (sw_twitter_item_stream_parent_class)->finalize (object);
}

static char *
_make_date (const char *s)
{
  struct tm tm;
  strptime (s, "%a %b %d %T %z %Y", &tm);
  return sw_time_t_to_string (timegm (&tm));
}

static SwItem *
_create_item_from_node (JsonNode *node)
{
  SwItem *item;
  JsonObject *root_o, *user_o, *geo_o = NULL;
  gchar *url;

  root_o = json_node_get_object (node);
  user_o = json_object_get_object_member (root_o, "user");

  if (json_object_has_member (root_o, "geo") &&
      !json_object_get_null_member (root_o, "geo"))
    geo_o = json_object_get_object_member (root_o, "geo");

  item = sw_item_new ();
  sw_item_put (item,
                "authorid",
                json_object_get_string_member (user_o, "screen_name"));

  url = g_strdup_printf ("http://twitter.com/%s/statuses/%lld", 
                         json_object_get_string_member (user_o, "screen_name"),
                         (long long int)json_object_get_int_member (root_o, "id"));
  sw_item_put (item, "id", url);
  sw_item_take (item, "url", url); /* steal string ownership */

  sw_item_put (item,
               "author",
               json_object_get_string_member (user_o, "name"));

  sw_item_put (item,
               "content",
               json_object_get_string_member (root_o, "text"));


  sw_item_take (item,
                "date",
                _make_date (json_object_get_string_member (root_o, "created_at")));

  if (json_object_has_member (user_o, "profile_image_url"))
  {
    sw_item_request_image_fetch (item,
                               TRUE,
                               "authoricon",
                               json_object_get_string_member (user_o, "profile_image_url"));
  }



  if (geo_o)
  {
    JsonArray *coords;

    coords = json_object_get_array_member (geo_o, "coordinates");
    sw_item_take (item,
                  "latitude",
                  g_strdup_printf ("%f",
                                   json_array_get_double_element (coords, 0)));
    sw_item_take (item,
                  "longitude",
                  g_strdup_printf ("%f",
                                   json_array_get_double_element (coords, 1)));
  }

  return item;
}

static void
_call_continous_cb (RestProxyCall *call,
                    const gchar   *buf,
                    gsize          len,
                    const GError  *error_in,
                    GObject       *weak_object,
                    gpointer       userdata)
{
  SwItemStream *item_stream = SW_ITEM_STREAM (weak_object);
  SwTwitterItemStreamPrivate *priv = GET_PRIVATE (weak_object);
  gint message_length;
  GError *error = NULL;

  if (error_in)
  {
    g_critical (G_STRLOC ": Error: %s", error_in->message);
    return;
  }

  if (!priv->cur_buffer)
  {
    priv->cur_buffer = g_string_new (NULL);
    priv->buf_size = 0;
  }

  g_debug (G_STRLOC ": Adding %d to string buffer", (gint)len);
  priv->cur_buffer = g_string_append_len (priv->cur_buffer,
                                          buf,
                                          len);
  priv->buf_size += len;

  /* Get rid of any preceding new lines */
  while (priv->cur_buffer->str[0]=='\r') {
    priv->cur_buffer = g_string_erase (priv->cur_buffer, 0, 2);
    g_debug (G_STRLOC ": filtering new lines");
  }

  /* Format is <message byte count>\r\n */
  while (sscanf (priv->cur_buffer->str, "%d\r\n", &message_length) == 1)
  {
    const gchar *message_buf = g_utf8_strchr (priv->cur_buffer->str,
                                              priv->buf_size,
                                              '\n');
    gint newline_pos = message_buf - priv->cur_buffer->str;

    g_debug (G_STRLOC ": newline pos = %d, message_length = %d, priv->buf_size = %d",
             newline_pos, message_length, priv->buf_size);

    if (priv->buf_size >= newline_pos + 1 + message_length)
    {
      g_debug (G_STRLOC ": Consuming data!");

      priv->cur_buffer = g_string_erase (priv->cur_buffer, 0, newline_pos + 1);

      if (!json_parser_load_from_data (priv->parser,
                                       priv->cur_buffer->str,
                                       message_length,
                                       &error))
      {
        g_warning (G_STRLOC ": error parsing json: %s", error->message);
      } else {
        SwItem *item;
        SwService *service;
        const gchar *content;
        const gchar *track_params;

        item = _create_item_from_node (json_parser_get_root (priv->parser));
        service = sw_item_stream_get_service (SW_ITEM_STREAM (item_stream));

        /* Check if this item actually matches the track parameter due to
         * needing to substitute spaces for commas to give a union
         */
        content = sw_item_get (item, "content");

        track_params = g_hash_table_lookup (priv->params, "keywords");

        if (strstr (content, track_params))
        {
          sw_item_set_service (item, service);
          sw_item_stream_add_item (item_stream, item);
          g_object_unref (item);
        } else {
          g_object_unref (item);
        }
      }

      priv->cur_buffer = g_string_erase (priv->cur_buffer, 0, message_length);
      priv->buf_size = priv->buf_size - (newline_pos + 1 + message_length);
    } else {
      break;
    }
  }
}

static void
twitter_item_stream_start (SwItemStream *item_stream)
{
  SwTwitterItemStreamPrivate *priv = GET_PRIVATE (item_stream);
  RestProxyCall *call;
  gchar *track_params;

  call = rest_proxy_new_call (priv->proxy);
  g_object_set (priv->proxy, "url-format", "http://stream.twitter.com/", NULL);

  rest_proxy_call_set_function (call, "1/statuses/filter.json");
  rest_proxy_call_set_method (call, "POST");

  track_params = g_strdup (g_hash_table_lookup (priv->params, "keywords"));

  if (!track_params)
  {
    g_critical (G_STRLOC ": Must have 'keywords' for filter");
    return;
  }

  /*
   * We have to convert spaces to commas and then filter the received data on
   * the version with spaces due to limitations in the twitter API
   */
  track_params = g_strdelimit (track_params, " ", ',');

  rest_proxy_call_add_param (call, "track", track_params);
  rest_proxy_call_add_param (call, "delimited", "length");

  rest_proxy_call_continuous (call,
                              _call_continous_cb,
                              (GObject *)item_stream,
                              NULL,
                              NULL);

  g_free (track_params);
}

static void
sw_twitter_item_stream_class_init (SwTwitterItemStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwItemStreamClass *item_stream_class = SW_ITEM_STREAM_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (SwTwitterItemStreamPrivate));

  object_class->get_property = sw_twitter_item_stream_get_property;
  object_class->set_property = sw_twitter_item_stream_set_property;
  object_class->dispose = sw_twitter_item_stream_dispose;
  object_class->finalize = sw_twitter_item_stream_finalize;

  item_stream_class->start = twitter_item_stream_start;
//  item_stream_class->stop = twitter_item_stream_stop;

  pspec = g_param_spec_object ("proxy",
                               "proxy",
                               "proxy",
                               REST_TYPE_PROXY,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PROXY, pspec);


  pspec = g_param_spec_string ("query",
                               "query",
                               "query",
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_QUERY, pspec);


  pspec = g_param_spec_boxed ("params",
                              "params",
                              "params",
                              G_TYPE_HASH_TABLE,
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_PARAMS, pspec);
}

static void
sw_twitter_item_stream_init (SwTwitterItemStream *self)
{
  SwTwitterItemStreamPrivate *priv = GET_PRIVATE (self);

  priv->parser = json_parser_new ();
}
