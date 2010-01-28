/*
 * libsocialweb - social data store
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

#ifndef _SW_CLIENT_LASTFM
#define _SW_CLIENT_LASTFM

#include <glib-object.h>

G_BEGIN_DECLS

#define SW_TYPE_CLIENT_LASTFM sw_client_lastfm_get_type()

#define SW_CLIENT_LASTFM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SW_TYPE_CLIENT_LASTFM, SwClientLastfm))

#define SW_CLIENT_LASTFM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SW_TYPE_CLIENT_LASTFM, SwClientLastfmClass))

#define SW_IS_CLIENT_LASTFM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SW_TYPE_CLIENT_LASTFM))

#define SW_IS_CLIENT_LASTFM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SW_TYPE_CLIENT_LASTFM))

#define SW_CLIENT_LASTFM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SW_TYPE_CLIENT_LASTFM, SwClientLastfmClass))

typedef struct {
  GObject parent;
} SwClientLastfm;

typedef struct {
  GObjectClass parent_class;
} SwClientLastfmClass;

GType sw_client_lastfm_get_type (void);

SwClientLastfm *sw_client_lastfm_new (void);
void sw_client_lastfm_now_playing (SwClientLastfm *lastfm,
                                       const char         *artist,
                                       const char         *album,
                                       const char         *track,
                                       guint32             length,
                                       guint32             tracknumber,
                                       const char         *musicbrainz_id);
void sw_client_lastfm_submit_track (SwClientLastfm *lastfm,
                                        const char         *artist,
                                        const char         *album,
                                        const char         *track,
                                        guint64             time,
                                        const char         *source,
                                        const char         *rating,
                                        guint32             length,
                                        guint32             tracknumber,
                                        const char         *musicbrainz_id);


G_END_DECLS

#endif /* _SW_CLIENT_LASTFM */

