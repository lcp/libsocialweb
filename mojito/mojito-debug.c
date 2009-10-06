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

#include <config.h>
#include <glib.h>

#include "mojito-debug.h"

guint mojito_debug_flags;

void
mojito_debug_init (const char *string)
{
  static gboolean setup_done = FALSE;
  static const GDebugKey keys[] = {
    { "main-loop", MOJITO_DEBUG_MAIN_LOOP },
    { "views", MOJITO_DEBUG_VIEWS },
    { "online", MOJITO_DEBUG_ONLINE }
  };

  if (G_LIKELY (setup_done))
    return;

  mojito_debug_flags = g_parse_debug_string (string, keys, G_N_ELEMENTS (keys));

  setup_done = TRUE;
}
