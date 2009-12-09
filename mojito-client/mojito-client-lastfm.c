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

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <interfaces/lastfm-bindings.h>
#include "mojito-client-lastfm.h"


G_DEFINE_TYPE (MojitoClientLastfm, mojito_client_lastfm, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_CLIENT_LASTFM, MojitoClientLastfmPrivate))

typedef struct _MojitoClientLastfmPrivate MojitoClientLastfmPrivate;

struct _MojitoClientLastfmPrivate {
    DBusGConnection *connection;
    DBusGProxy *proxy;
};

#define MOJITO_SERVICE_NAME "com.intel.Mojito"
#define MOJITO_SERVICE_LASTFM_INTERFACE "com.intel.Mojito.Service.Lastfm"

static void
mojito_client_lastfm_dispose (GObject *object)
{
  MojitoClientLastfmPrivate *priv = GET_PRIVATE (object);

  if (priv->connection)
  {
    dbus_g_connection_unref (priv->connection);
    priv->connection = NULL;
  }

  if (priv->proxy)
  {
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
  }

  G_OBJECT_CLASS (mojito_client_lastfm_parent_class)->dispose (object);
}

static void
mojito_client_lastfm_constructed (GObject *object)
{
  MojitoClientLastfmPrivate *priv = GET_PRIVATE (object);
  GError *error = NULL;
  DBusConnection *conn;
  DBusError derror;

  priv->connection = dbus_g_bus_get (DBUS_BUS_STARTER, &error);

  if (!priv->connection)
  {
    g_critical (G_STRLOC ": Error getting DBUS connection: %s",
                error->message);
    g_clear_error (&error);
    return;
  }

  conn = dbus_g_connection_get_connection (priv->connection);
  dbus_error_init (&derror);
  if (!dbus_bus_start_service_by_name (conn,
                                       MOJITO_SERVICE_NAME,
                                       0,
                                       NULL,
                                       &derror))
  {
    g_critical (G_STRLOC ": Error starting mojito service: %s",
                derror.message);
    dbus_error_free (&derror);
    return;
  }

  priv->proxy = dbus_g_proxy_new_for_name_owner (priv->connection,
                                                 MOJITO_SERVICE_NAME,
                                                 "/com/intel/Mojito/Service/twitter",
                                                 MOJITO_SERVICE_LASTFM_INTERFACE,
                                                 &error);

  if (!priv->proxy)
  {
    g_critical (G_STRLOC ": Error setting up proxy for remote object: %s",
                error->message);
    g_clear_error (&error);
    return;
  }
}

static void
mojito_client_lastfm_class_init (MojitoClientLastfmClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoClientLastfmPrivate));

  object_class->dispose = mojito_client_lastfm_dispose;
  object_class->constructed = mojito_client_lastfm_constructed;
}

static void
mojito_client_lastfm_init (MojitoClientLastfm *self)
{
}

MojitoClientLastfm *
mojito_client_lastfm_new (void)
{
  return g_object_new (MOJITO_TYPE_CLIENT_LASTFM, NULL);
}

static void
_mojito_client_lastfm_now_playing_cb (DBusGProxy     *proxy,
                                      GError         *error,
                                      gpointer        userdata)
{
  if (error)
  {
    g_warning (G_STRLOC ": Error when setting now playing: %s",
               error->message);
    g_error_free (error);
  }
}

void
mojito_client_lastfm_now_playing (MojitoClientLastfm *lastfm,
                                  const char         *artist,
                                  const char         *album,
                                  const char         *track,
                                  guint32             length,
                                  guint32             tracknumber,
                                  const char         *musicbrainz_id)
{
  MojitoClientLastfmPrivate *priv = GET_PRIVATE (lastfm);

  com_intel_Mojito_Service_Lastfm_now_playing_async (priv->proxy, artist,
                                                     album, track,
                                                     length, tracknumber,
                                                     musicbrainz_id,
                                                     _mojito_client_lastfm_now_playing_cb,
                                                     NULL);
}

static void
_mojito_client_lastfm_submit_track_cb (DBusGProxy     *proxy,
                                       GError         *error,
                                       gpointer        userdata)
{
  if (error)
  {
    g_warning (G_STRLOC ": Error when submitting track: %s",
               error->message);
    g_error_free (error);
  }
}

void
mojito_client_lastfm_submit_track (MojitoClientLastfm *lastfm,
                                   const char         *artist,
                                   const char         *album,
                                   const char         *track,
                                   guint64             time,
                                   const char         *source,
                                   const char         *rating,
                                   guint32             length,
                                   guint32             tracknumber,
                                   const char         *musicbrainz_id)
{
  MojitoClientLastfmPrivate *priv = GET_PRIVATE (lastfm);

  com_intel_Mojito_Service_Lastfm_submit_track_async (priv->proxy, artist,
                                                      album, track,
                                                      time, source, rating,
                                                      length, tracknumber,
                                                      musicbrainz_id,
                                                      _mojito_client_lastfm_submit_track_cb,
                                                      NULL);
}
