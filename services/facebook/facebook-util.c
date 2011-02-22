/*
 * Facebook service utility functions
 *
 * Copyright (C) 2010-2011 Collabora Ltd.
 *
 * Authors: Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
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

#include <libsoup/soup.h>

#include <libsocialweb/sw-service.h>

#include "facebook-util.h"

char *
build_picture_url (RestProxy *proxy,
                   char *object,
                   char *size)
{
  char *base_url = NULL, *pic_url = NULL;
  g_object_get (proxy, "url-format", &base_url, NULL);
  pic_url = g_strdup_printf ("%s/%s/picture?type=%s",
                             base_url, object, size);
  g_free (base_url);

  return pic_url;
}

JsonNode *
json_node_from_call (RestProxyCall *call, GError** error)
{
  JsonNode *root;
  JsonObject *object = NULL;
  char *error_message = NULL;
  JsonParser *parser = NULL;

  g_return_val_if_fail (call, NULL);

  if (!SOUP_STATUS_IS_SUCCESSFUL (rest_proxy_call_get_status_code (call))) {
    g_set_error (error, SW_SERVICE_ERROR,
                 SW_SERVICE_ERROR_REMOTE_ERROR,
                 "Error from Facebook: %s (%d)",
                 rest_proxy_call_get_status_message (call),
                 rest_proxy_call_get_status_code (call));
    g_object_unref (parser);
    return NULL;
  }

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser,
                                   rest_proxy_call_get_payload (call),
                                   rest_proxy_call_get_payload_length (call),
                                   NULL)) {
    g_set_error (error, SW_SERVICE_ERROR,
                 SW_SERVICE_ERROR_REMOTE_ERROR,
                 "Malformed JSON from Facebook: %s",
                 rest_proxy_call_get_payload (call));
    g_object_unref (parser);
    return NULL;
  }

  root = json_parser_get_root (parser);

  if (root)
    root = json_node_copy (root);

  g_object_unref (parser);

  if (root == NULL) {
    g_set_error (error, SW_SERVICE_ERROR,
                 SW_SERVICE_ERROR_REMOTE_ERROR,
                 "Error from Facebook: %s",
                 rest_proxy_call_get_payload (call));
    return NULL;
  }

  /*
   * Is it an error?  If so, it'll be a hash containing
   * the key "error", which maps to a hash containing
   * a key "message".
   */

  if (json_node_get_node_type (root) == JSON_NODE_OBJECT) {
    object = json_node_get_object (root);
  }

  if (object && json_object_has_member (object, "error")) {
    JsonNode *inner = json_object_get_member (object,
                                              "error");
    JsonObject *inner_object = NULL;

    if (json_node_get_node_type (inner) == JSON_NODE_OBJECT)
      inner_object = json_node_get_object (inner);

    if (inner_object && json_object_has_member (inner_object, "message"))
      error_message = get_child_node_value (inner, "message");
  }

  if (error_message) {
    g_set_error (error, SW_SERVICE_ERROR,
                 SW_SERVICE_ERROR_REMOTE_ERROR,
                 "Error response from Facebook: %s", error_message);
    g_free (error_message);
    json_node_free (root);
    return NULL;
  } else {
    return root;
  }
}

/*
 * For a given parent @node, get the child node called @name and return a copy
 * of the content, or NULL. If the content is the empty string, NULL is
 * returned.
 */
char *
get_child_node_value (JsonNode *node, const char *name)
{
  JsonNode *subnode;
  JsonObject *object;
  GValue value = {0};
  const char *string;
  char *result = NULL;

  if (!node || !name)
    return NULL;

  if (json_node_get_node_type (node) == JSON_NODE_OBJECT) {
    object = json_node_get_object (node);
  } else {
    return NULL;
  }

  if (!json_object_has_member (object, name)) {
    return NULL;
  }

  subnode = json_object_get_member (object, name);

  if (!subnode)
    return NULL;

  json_node_get_value (subnode, &value);

  string = g_value_get_string (&value);

  if (string && string[0]) {
    result = g_strdup (string);
  }

  g_value_unset (&value);

  return result;
}
