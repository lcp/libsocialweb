#! /usr/bin/env python

import sys
import dbus, gobject
from dbus.mainloop.glib import DBusGMainLoop

DBusGMainLoop(set_as_default=True)

bus = dbus.SessionBus()
bus.start_service_by_name("com.intel.Mojito")

mojito = bus.get_object("com.intel.Mojito", "/com/intel/Mojito")
mojito = dbus.Interface(mojito, "com.intel.Mojito")

sources = sys.argv[1:]
if not sources:
    sources = mojito.getSources()

path = mojito.openView(sources, 10)
view = bus.get_object("com.intel.Mojito", path)
view = dbus.Interface(view, "com.intel.Mojito.View")

def added(source, uuid, date, item):
    print source, uuid, date, item
view.connect_to_signal("ItemAdded", added)

def removed(source, uuid):
    print source, uuid
view.connect_to_signal("ItemRemoved", removed)

view.start()

loop = gobject.MainLoop()
loop.run()
