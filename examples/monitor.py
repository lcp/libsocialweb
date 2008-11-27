#! /usr/bin/env python

import dbus, gobject
from dbus.mainloop.glib import DBusGMainLoop

DBusGMainLoop(set_as_default=True)

bus = dbus.SessionBus()
bus.start_service_by_name("com.intel.Mojito")

mojito = bus.get_object("com.intel.Mojito", "/com/intel/Mojito")
mojito = dbus.Interface(mojito, "com.intel.Mojito")

path = mojito.openView(mojito.getSources())
view = bus.get_object("com.intel.Mojito", path)
view = dbus.Interface(view, "com.intel.Mojito.View")

def added(item, data):
    print item, data

view.connect_to_signal("ItemAdded", added)
view.start()

loop = gobject.MainLoop()
loop.run()
