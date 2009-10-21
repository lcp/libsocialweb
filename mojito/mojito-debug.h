/*
 * Mojito - social data store
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
  MOJITO_DEBUG_MAIN_LOOP = 1 << 0,
  MOJITO_DEBUG_VIEWS = 1 << 1,
  MOJITO_DEBUG_ONLINE = 1 << 2,
  MOJITO_DEBUG_ITEM = 1 << 3,
  MOJITO_DEBUG_TWITTER = 1 << 4
} MojitoDebugFlags;

extern guint mojito_debug_flags;

#define MOJITO_DEBUG_ENABLED(category) (mojito_debug_flags & MOJITO_DEBUG_##category)

#define MOJITO_DEBUG(category,x,a...)             G_STMT_START {      \
    if (MOJITO_DEBUG_ENABLED(category))                               \
      { g_message ("[" #category "] " G_STRLOC ": " x, ##a); }        \
  } G_STMT_END

void mojito_debug_init (const char *string);
