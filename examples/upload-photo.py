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

if len (sys.argv) != 3:
    print "$ upload-photo.py [service] [filename]"
    sys.exit(1)

DBusGMainLoop(set_as_default=True)
loop = gobject.MainLoop()

bus = dbus.SessionBus()
bus.start_service_by_name("com.meego.libsocialweb")

service = bus.get_object("com.meego.libsocialweb", "/com/meego/libsocialweb/Service/%s" % sys.argv[1])
photoupload = dbus.Interface(service, "com.meego.libsocialweb.PhotoUpload")

def progress_cb(opid, progress, message):
    # Ignore signals for other uploads
    if opid != this_opid:
        return

    if progress == -1:
        print "Error: %s" % message
        loop.quit()
    else:
        print "%d%% complete" % progress
        if progress == 100:
            loop.quit()
photoupload.connect_to_signal("PhotoUploadProgress", progress_cb)

this_opid = photoupload.UploadPhoto(sys.argv[2], {})

loop.run()
