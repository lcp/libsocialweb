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

#include <libsocialweb/sw-core.h>
#include <libsoup/soup.h>

SoupSession * sw_web_make_sync_session (void);

SoupSession * sw_web_make_async_session (void);

char * sw_web_download_image (const char *url);

/*
 * @url: the URL you requested
 * @file: the local file if the download was successful, otherwise NULL
 */
typedef void (*ImageDownloadCallback) (const char *url,
                                       char       *file,
                                       gpointer    user_data);

void sw_web_download_image_async (const char            *url,
                                  ImageDownloadCallback  callback,
                                  gpointer               user_data);
