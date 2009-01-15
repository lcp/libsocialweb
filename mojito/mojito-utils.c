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

#include "mojito-utils.h"
#include <libsoup/soup.h>

time_t
mojito_time_t_from_string (const char *s)
{
  SoupDate *date;
  time_t t;

  date = soup_date_new_from_string (s);
  t = soup_date_to_time_t (date);
  soup_date_free (date);

  return t;
}

char *
mojito_time_t_to_string (time_t t)
{
  SoupDate *date;
  char *s;

  date = soup_date_new_from_time_t (t);
  s = soup_date_to_string (date, SOUP_DATE_HTTP);
  soup_date_free (date);

  return s;
}

char *
mojito_date_to_iso (const char *s)
{
  SoupDate *date;
  char *iso;

  date = soup_date_new_from_string (s);
  iso = soup_date_to_string (date, SOUP_DATE_ISO8601);
  soup_date_free (date);

  return iso;
}
