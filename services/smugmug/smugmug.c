/*
 * libsocialweb SmugMug service support
 *
 * Copyright (C) 2011 Collabora Ltd.
 *
 * Authors: Eitan Isaacson <eitan.isaacson@collabora.co.uk>
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
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gnome-keyring.h>

#include <libsocialweb/sw-service.h>
#include <libsocialweb/sw-item.h>
#include <libsocialweb/sw-utils.h>
#include <libsocialweb/sw-web.h>
#include <libsocialweb/sw-call-list.h>
#include <libsocialweb/sw-debug.h>
#include <libsocialweb/sw-client-monitor.h>
#include <libsocialweb/sw-online.h>
#include <libsocialweb-keyfob/sw-keyfob.h>
#include <libsocialweb-keystore/sw-keystore.h>

#include <rest/oauth-proxy.h>
#include <rest/rest-xml-parser.h>

#include <interfaces/sw-collections-ginterface.h>

#include "smugmug.h"

#define OAUTH_URL "http://api.smugmug.com/services/oauth/"
#define REST_URL "https://secure.smugmug.com/services/api/rest/1.2.2/"

static void collections_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SwServiceSmugmug, sw_service_smugmug, SW_TYPE_SERVICE,
                         G_IMPLEMENT_INTERFACE (SW_TYPE_COLLECTIONS_IFACE,
                                                collections_iface_init));

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SW_TYPE_SERVICE_SMUGMUG, SwServiceSmugmugPrivate))

struct _SwServiceSmugmugPrivate {
  RestProxy *proxy;
};

static const char *
get_name (SwService *service)
{
  return "smugmug";
}

static const char **
get_static_caps (SwService *service)
{
  static const char * caps[] = {
    HAS_BANISHABLE_IFACE,

    NULL
  };

  return caps;
}

static void
got_tokens_cb (RestProxy *proxy, gboolean authorised, gpointer user_data)
{
  SwServiceSmugmug *self = (SwServiceSmugmug *) user_data;
  SwServiceSmugmugPrivate *priv = self->priv;

  SW_DEBUG (SMUGMUG, "%sauthorized", authorised ? "" : "un");

  g_object_set (G_OBJECT (priv->proxy), "url-format", REST_URL, NULL);
}

static void
online_notify (gboolean online, gpointer user_data)
{
  SwServiceSmugmug *self = (SwServiceSmugmug *) user_data;
  SwServiceSmugmugPrivate *priv = self->priv;

  if (online) {
    g_object_set (G_OBJECT (priv->proxy), "url-format", OAUTH_URL, NULL);
    sw_keyfob_oauth ((OAuthProxy *)priv->proxy, got_tokens_cb, self);
  }
}

/*
 * The credentials have been updated (or we're starting up), so fetch them from
 * the keyring.
 */
static void
refresh_credentials (SwServiceSmugmug *smugmug)
{
  SW_DEBUG (SMUGMUG, "Credentials updated");

  online_notify (FALSE, (SwService *) smugmug);
  online_notify (TRUE, (SwService *) smugmug);
}

/*
 * SwService:credentials_updated vfunc implementation
 */
static void
credentials_updated (SwService *service)
{
  refresh_credentials (SW_SERVICE_SMUGMUG (service));
}

static void
sw_service_smugmug_dispose (GObject *object)
{
  SwServiceSmugmugPrivate *priv = ((SwServiceSmugmug*)object)->priv;

  if (priv->proxy) {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (sw_service_smugmug_parent_class)->dispose (object);
}

static void
collections_iface_init (gpointer g_iface,
                  gpointer iface_data)
{
}

static void
sw_service_smugmug_class_init (SwServiceSmugmugClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwServiceClass *service_class = SW_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SwServiceSmugmugPrivate));

  object_class->dispose = sw_service_smugmug_dispose;

  service_class->get_name = get_name;
  service_class->get_static_caps = get_static_caps;
  service_class->credentials_updated = credentials_updated;
}

static void
sw_service_smugmug_init (SwServiceSmugmug *self)
{
  SwServiceSmugmugPrivate *priv;
  const gchar *key = NULL;
  const gchar *secret = NULL;

  priv = self->priv = GET_PRIVATE (self);

  sw_keystore_get_key_secret ("smugmug", &key, &secret);

  priv->proxy = oauth_proxy_new (key, secret, REST_URL, FALSE);

  sw_online_add_notify (online_notify, self);

  refresh_credentials (self);
}
