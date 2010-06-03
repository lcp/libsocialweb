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

#include <config.h>
#include <glib.h>

#include "sw-debug.h"

guint sw_debug_flags;

void
sw_debug_init (const char *string)
{
  static gboolean setup_done = FALSE;
  static const GDebugKey keys[] = {
    { "main-loop", SW_DEBUG_MAIN_LOOP },
    { "views", SW_DEBUG_VIEWS },
    { "online", SW_DEBUG_ONLINE },
    { "item", SW_DEBUG_ITEM },
    { "twitter", SW_DEBUG_TWITTER },
    { "myspace", SW_DEBUG_MYSPACE },
    { "lastfm", SW_DEBUG_LASTFM },
    { "core", SW_DEBUG_CORE }
  };

  if (G_LIKELY (setup_done))
    return;

  sw_debug_flags = g_parse_debug_string (string, keys, G_N_ELEMENTS (keys));

  setup_done = TRUE;
}
