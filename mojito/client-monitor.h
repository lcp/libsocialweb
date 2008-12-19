#include <glib-object.h>
#include <dbus/dbus-glib.h>

void client_monitor_init (DBusGConnection *connection);

void client_monitor_add (char *sender, GObject *object);

void client_monitor_remove (char *sender, GObject *object);
