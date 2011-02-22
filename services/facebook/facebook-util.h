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

#ifndef _FACEBOOK_UTIL_H
#define _FACEBOOK_UTIL_H

#include <glib.h>
#include <rest/rest-proxy.h>
#include <rest/rest-proxy-call.h>
#include <json-glib/json-glib.h>

#define FB_PICTURE_SIZE_SQUARE "square"
#define FB_PICTURE_SIZE_SMALL "small"
#define FB_PICTURE_SIZE_LARGE "large"

/* Builds a url to a facebook avatar for the given object */
char* build_picture_url (RestProxy *proxy, char *object, char *size);
/* utility functions for handling json responses from facebook */
JsonNode * json_node_from_call (RestProxyCall *call, GError** error);
char * get_child_node_value (JsonNode *node, const char *name);

#endif /* _FACEBOOK_UTIL_H */
