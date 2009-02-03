#! /usr/bin/env python

# Mojito - social data store
# Copyright (C) 2008 - 2009 Intel Corporation.
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

import sys
import dbus, gobject
from dbus.mainloop.glib import DBusGMainLoop

DBusGMainLoop(set_as_default=True)

bus = dbus.SessionBus()
bus.start_service_by_name("com.intel.Mojito")

mojito = bus.get_object("com.intel.Mojito", "/com/intel/Mojito")
mojito = dbus.Interface(mojito, "com.intel.Mojito")

services = sys.argv[1:]
if not services:
    services = mojito.GetServices()

path = mojito.OpenView(services, 10)
view = bus.get_object("com.intel.Mojito", path)
view = dbus.Interface(view, "com.intel.Mojito.View")

def added(service, uuid, date, item):
    print service, uuid, date, item
view.connect_to_signal("ItemAdded", added)

def removed(service, uuid):
    print service, uuid
view.connect_to_signal("ItemRemoved", removed)

view.Start()

loop = gobject.MainLoop()
loop.run()
