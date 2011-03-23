/*
 * libsocialweb - social data store
 * Copyright (C) 2009 Intel Corporation.
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

#include <glib.h>

typedef enum {
  SW_DEBUG_MAIN_LOOP = 1 << 0,
  SW_DEBUG_VIEWS = 1 << 1,
  SW_DEBUG_ONLINE = 1 << 2,
  SW_DEBUG_ITEM = 1 << 3,
  SW_DEBUG_CONTACT = 1 << 4,
  SW_DEBUG_TWITTER = 1 << 5,
  SW_DEBUG_LASTFM = 1 << 6,
  SW_DEBUG_CORE = 1 << 7,
  SW_DEBUG_VIMEO = 1 << 8,
  SW_DEBUG_FLICKR = 1 << 9,
  SW_DEBUG_SMUGMUG = 1 << 10,
  SW_DEBUG_PHOTOBUCKET = 1 << 11,
  SW_DEBUG_FACEBOOK = 1 << 12,
  SW_DEBUG_CLIENT_MONITOR = 1 << 13
} SwDebugFlags;

extern guint sw_debug_flags;

#define SW_DEBUG_ENABLED(category) (sw_debug_flags & SW_DEBUG_##category)

#define SW_DEBUG(category,x,a...)             G_STMT_START {      \
    if (SW_DEBUG_ENABLED(category))                               \
      { g_message ("[" #category "] " G_STRLOC ": " x, ##a); }        \
  } G_STMT_END

void sw_debug_init (const char *string);
