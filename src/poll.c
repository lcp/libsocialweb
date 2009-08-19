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

#include <string.h>
#include <unistd.h>
#include <glib.h>

static GTimeVal last;
static GPollFunc func;

static long
time_val_difference (GTimeVal *a, GTimeVal *b)
{
  return ((a->tv_sec - b->tv_sec) * G_USEC_PER_SEC) + (a->tv_usec - b->tv_usec);
}

static gint
my_poll (GPollFD *ufds, guint nfsd, gint timeout_)
{
  GTimeVal now;
  long diff;
  gint ret;

  g_get_current_time (&now);

  diff = time_val_difference (&now, &last);

  /* Log if the delay was more than a tenth of a second */
  if (diff > (G_USEC_PER_SEC / 10)) {
    g_debug ("Main loop blocked for %g seconds", (double)diff / G_USEC_PER_SEC);
  }

  ret = func (ufds, nfsd, timeout_);

  g_get_current_time (&now);
  last = now;

  return ret;
}

void
poll_init (void)
{
  g_get_current_time (&last);

  func = g_main_context_get_poll_func (g_main_context_default ());
  g_main_context_set_poll_func (g_main_context_default (), my_poll);
}
