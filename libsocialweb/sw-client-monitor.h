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

#ifndef _SW_CLIENT_MONITOR
#define _SW_CLIENT_MONITOR

#include <glib-object.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

void sw_client_monitor_init (DBusGConnection *connection);
void sw_client_monitor_add (char *sender, GObject *object);
void sw_client_monitor_remove (char *sender, GObject *object);

G_END_DECLS

#endif /* _SW_CLIENT_MONITOR */
