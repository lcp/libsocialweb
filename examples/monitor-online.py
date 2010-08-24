#! /usr/bin/env python

# libsocialweb - social data store
# Copyright (C) 2010 Intel Corporation.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU Lesser General Public License,
# version 2.1, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
# more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

import dbus, gobject
from dbus.mainloop.glib import DBusGMainLoop

DBusGMainLoop(set_as_default=True)

bus = dbus.SessionBus()
bus.start_service_by_name("com.meego.libsocialweb")

sw = bus.get_object("com.meego.libsocialweb", "/com/meego/libsocialweb")
sw = dbus.Interface(sw, "com.meego.libsocialweb")

def online(state):
    if state:
        print "Currently online"
    else:
        print "Currently offline"

online(sw.IsOnline())
sw.connect_to_signal("OnlineChanged", online)

loop = gobject.MainLoop()
loop.run()
