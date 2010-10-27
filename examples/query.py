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

import sys, time
import dbus, gobject
from dbus.mainloop.glib import DBusGMainLoop

DBusGMainLoop(set_as_default=True)

bus = dbus.SessionBus()
bus.start_service_by_name("com.meego.libsocialweb")

service = bus.get_object("com.meego.libsocialweb", "/com/meego/libsocialweb/Service/%s" % sys.argv[1])

if len(sys.argv) > 2:
    query = sys.argv[2]
else:
    query = "feed"
path = service.OpenView(query, {}, dbus_interface="com.meego.libsocialweb.Query")
view = bus.get_object("com.meego.libsocialweb", path)
view = dbus.Interface(view, "com.meego.libsocialweb.ItemView")

def now():
    return time.strftime("%T", time.localtime())

def added(items, member=None):
    for (service, uuid, date, item) in items:
        print "[%s] %s: Item %s from %s" % (now(), member, uuid, service)
        print "Timestamp: %s" % time.strftime("%c", time.gmtime(date))
        for key in item:
            print "  %s: %s" % (key, item[key])
        print
view.connect_to_signal("ItemsAdded", added, member_keyword="member")
view.connect_to_signal("ItemsChanged", added, member_keyword="member")

def removed(items):
    for (service, uuid) in items:
        print "[%s] Item %s removed from %s" % (now(), uuid, service)
        print
view.connect_to_signal("ItemsRemoved", removed)

view.Start()

loop = gobject.MainLoop()
loop.run()
