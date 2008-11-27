#! /usr/bin/env python

import dbus
from dbus.mainloop.glib import DBusGMainLoop

DBusGMainLoop(set_as_default=True)

bus = dbus.SessionBus()

mojito = bus.get_object("com.intel.Mojito", "/com/intel/Mojito")
mojito = dbus.Interface(mojito, "com.intel.Mojito")

print mojito.getSources()
