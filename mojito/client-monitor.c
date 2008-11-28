#include <string.h>
#include <glib-object.h>
#include <dbus/dbus-glib.h>

/* Hash of client addresses to GList of GObjects */
static GHashTable *clients;

static void
name_owner_changed (DBusGProxy *proxy,
                    const char *name,
                    const char *prev_owner,
                    const char *new_owner,
                    gpointer user_data)
{
  /* If a client we are tracking has disappeared, then unref all of the objects
     that they are connected to. */
  if (strcmp (new_owner, "") == 0 && strcmp (name, prev_owner) == 0) {
    GList *list;
    
    list = g_hash_table_lookup (clients, prev_owner);
    
    while (list) {
      g_object_unref (list->data);
      list = g_list_delete_link (list, list);
    }
    
    g_hash_table_remove (clients, prev_owner);
  }
}

/* init structures, listen to nameownerchanged */
void
client_monitor_init (DBusGConnection *connection)
{
  DBusGProxy *bus;
  
  clients = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  
  bus = dbus_g_proxy_new_for_name (connection,
                                   DBUS_SERVICE_DBUS,
                                   DBUS_PATH_DBUS,
                                   DBUS_INTERFACE_DBUS);
  
  dbus_g_proxy_add_signal (bus, "NameOwnerChanged",
                           G_TYPE_STRING,
                           G_TYPE_STRING,
                           G_TYPE_STRING,
                           G_TYPE_INVALID);
  
  dbus_g_proxy_connect_signal (bus,
                               "NameOwnerChanged",
                               G_CALLBACK (name_owner_changed),
                               NULL, NULL);
}

/* @sender has connected to @object.  If @sender disconnects from the bus,
   unref @object.  takes ownership of sender */
void
client_monitor_add (char *sender, GObject *object)
{  
  GList *list;
  
  g_return_if_fail (sender);
  g_return_if_fail (G_IS_OBJECT (object));

  // TODO? g_object_weak_ref (object, weak_notify, NULL);
  
  list = g_hash_table_lookup (clients, sender);
  list = g_list_prepend (list, object);
  g_hash_table_insert (clients, sender, list);
}

/* @sender has disconnected from @object.  Takes ownership of sender. */
void
client_monitor_remove (char *sender, GObject *object)
{
  GList *list;
  list = g_hash_table_lookup (clients, sender);
  list = g_list_remove (list, object);
  /* This will cause sender to be freed */
  g_hash_table_insert (clients, sender, list);
}
