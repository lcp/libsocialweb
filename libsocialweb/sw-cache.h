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

#ifndef _SW_CACHE
#define _SW_CACHE

#include <libsocialweb/sw-service.h>
#include <libsocialweb/sw-set.h>

G_BEGIN_DECLS

void sw_cache_save (SwService   *service,
                    const gchar *query,
                    GHashTable  *params,
                    SwSet       *set);

SwSet *sw_cache_load (SwService   *service,
                      const gchar *query,
                      GHashTable  *params,
                      SwSet* (*set_constr)());

void sw_cache_drop (SwService   *service,
                    const gchar *query,
                    GHashTable  *params);

void sw_cache_drop_all (SwService *service);

char *make_relative_path (const char *key, const char *value);

G_END_DECLS

#endif /* _SW_CACHE */
