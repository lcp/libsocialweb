#include <glib-object.h>
#include <libsocialweb/sw-online.h>

int
main (int argc, char **argv)
{
  g_type_init ();
  if (sw_is_online ()) {
    g_print ("Online\n");
  } else {
    g_print ("Offline\n");
  }
}
